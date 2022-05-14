/*
 * Copyright (c) 2021 Pavel Shramov <shramov@mexmat.net>
 *
 * tll is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#include "App.h"

#include <variant>

#include "tll/channel/base.h"
#include "tll/channel/module.h"
#include "tll/util/cppring.h"
#include "tll/util/size.h"

#include "http-scheme-binder.h"
#include "ev-backend.h"

#ifdef __linux__
#include <sys/timerfd.h>
#include <unistd.h>
#endif

#include <uv.h>

using namespace tll;

class WSHTTP;
class WSWS;
class WSPub;

class WSServer : public tll::channel::Base<WSServer>
{
	using node_ptr_t = std::variant<WSHTTP *, WSWS *, WSPub *>;
 	std::map<std::string_view, node_ptr_t, std::less<>> _nodes;

	int _timerfd = -1;

	static thread_local WSServer * _instance;

	std::unique_ptr<uWS::App> _app;
	us_listen_socket_t * _app_socket;
	uWS::Loop * _app_loop;

	uv_loop_t _uv_loop = {};
	uv_poll_t _uv_timer = {};

	std::string _host;
	unsigned short _port;

 public:
	struct user_t {
		std::variant<WSWS *, WSPub *> channel;
		tll::util::DataRing<void>::iterator position; // For pub nodes
		tll_addr_t addr;
	};

	static constexpr std::string_view channel_protocol() { return "ws"; }

	int _init(const tll::Channel::Url &, tll::Channel *master);
	int _open(const tll::ConstConfig &);
	int _close();
	void _free()
	{
		if (_instance == this)
			_instance = nullptr;
	}

	int _process(long timeout, int flags);

	template <typename T>
	int node_add(std::string_view prefix, T * ptr)
	{
		auto it = _nodes.find(prefix);
		if (it != _nodes.end())
			return EEXIST;
		_log.info("Add new {} node {} at {}", T::channel_protocol(), ptr->name, prefix);
		_nodes.emplace(prefix, ptr);
		return 0;
	}

	template <typename T>
	int node_remove(std::string_view prefix, T * ptr)
	{
		auto it = _nodes.find(prefix);
		if (it == _nodes.end())
			return ENOENT;
		if (!std::holds_alternative<T *>(it->second))
			return EINVAL;
		if (std::get<T *>(it->second) != ptr)
			return EINVAL;
		_nodes.erase(it);
		return 0;
	}

 private:

	enum class Method { GET, POST, PUT, HEAD, OPTIONS };

	template <Method M>
	void _http(uWS::HttpResponse<false> * resp, uWS::HttpRequest *req);
	void _ws_upgrade(uWS::HttpResponse<false> * resp, uWS::HttpRequest *req, us_socket_context_t *context);
	void _ws_open(uWS::WebSocket<false, true> *);
	void _ws_message(uWS::WebSocket<false, true> *, std::string_view, uWS::OpCode);
	void _ws_drain(uWS::WebSocket<false, true> *);
	void _ws_close(uWS::WebSocket<false, true> *, int code, std::string_view message);
};

thread_local WSServer * WSServer::_instance = nullptr;

template <typename T, typename R = uWS::HttpResponse<false>>
class WSNode : public tll::channel::Base<T>
{
 protected:
 	WSServer * _master = nullptr;
	using Base = tll::channel::Base<T>;

	std::string _prefix;

	std::map<uint64_t, R *> _sessions;
	tll_addr_t _addr;

	using user_t = WSServer::user_t;

 public:
	static constexpr std::string_view param_prefix() { return "ws"; }
	static constexpr auto process_policy() { return Base::ProcessPolicy::Never; }

	int _init(const tll::Channel::Url &, tll::Channel *master);
	int _open(const tll::ConstConfig &);
	int _close();

	int _post_data(R * resp, const tll_msg_t *msg, int flags)
	{
		resp->writeStatus(uWS::HTTP_200_OK);
		resp->end(std::string_view((const char *) msg->data, msg->size));
		return 0;
	}

	int _post_control(R * resp, const tll_msg_t *msg, int flags)
	{
		if (msg->msgid != http_scheme::disconnect<tll_msg_t>::meta_id())
			return 0;
		_disconnected(resp, msg->addr);
		resp->end();
		return 0;
	}

	int _post(const tll_msg_t *msg, int flags)
	{
		auto it = _sessions.find(msg->addr.u64);
		if (it == _sessions.end())
			return this->_log.fail(ENOENT, "Failed to post: session 0x{:x} not found", msg->addr.u64);

		if (msg->type == TLL_MESSAGE_DATA)
			return static_cast<T *>(this)->_post_data(it->second, msg, flags);
		else if (msg->type == TLL_MESSAGE_CONTROL)
			return static_cast<T *>(this)->_post_control(it->second, msg, flags);
		return 0;
	}

	int _connected(R * resp, std::string_view url, tll_addr_t * addr);
	int _disconnected(R * resp, tll_addr_t addr);

 protected:
	tll_addr_t _next_addr()
	{
		return { ++_addr.u64 }; // TODO: Check for duplicates
	}
};

class WSHTTP : public WSNode<WSHTTP>
{
 public:
	static constexpr std::string_view channel_protocol() { return "ws+http"; }
};

class WSWS : public WSNode<WSWS, uWS::WebSocket<false, true>>
{
 public:
 	using Response = uWS::WebSocket<false, true>;

	static constexpr std::string_view channel_protocol() { return "ws+ws"; }

	int _post_data(Response * resp, const tll_msg_t *msg, int flags)
	{
		resp->send(std::string_view((const char *) msg->data, msg->size));
		return 0;
	}

	int _post_control(Response * resp, const tll_msg_t *msg, int flags)
	{
		if (msg->msgid != http_scheme::disconnect<tll_msg_t>::meta_id())
			return 0;
		resp->end();
		_disconnected(nullptr, msg->addr);
		return 0;
	}
};

class WSPub : public WSNode<WSPub, uWS::WebSocket<false, true>>
{
 	tll::util::DataRing<void> _ring;
 public:
 	using Response = uWS::WebSocket<false, true>;
	using Parent = WSNode<WSPub, Response>;

	static constexpr std::string_view channel_protocol() { return "ws+pub"; }

	int _init(const tll::Channel::Url &url, tll::Channel *master)
	{
		if (auto r = Parent::_init(url, master); r)
			return r;

		auto reader = channel_props_reader(url);

		auto size = reader.getT<size_t>("ring-size", 1024);
		auto data = reader.getT<tll::util::Size>("data-size", 1024 * 1024);

		if (!reader)
			return _log.fail(EINVAL, "Invalid url: {}", reader.error());

		_ring.resize(size);
		_ring.data_resize(data);
		return 0;
	}

	int _open(const tll::ConstConfig &url)
	{
		_ring.clear();
		return Parent::_open(url);
	}

	int _post_data(Response * resp, const tll_msg_t *msg, int flags)
	{
		if (msg->size > _ring.data_capacity() / 2)
			return _log.fail(EINVAL, "Message size {} is larger then half of buffer: {}", msg->size, _ring.data_capacity());
		auto last = _ring.end();
		do {
			auto r = _ring.push_back(msg->data, msg->size);
			if (r != nullptr)
				break;
			auto first = _ring.begin();
			_ring.pop_front();

			auto it = _sessions.begin();
			while (it != _sessions.end()) {
				auto [addr, ws] = *(it++); // Close will remove element
				auto user = static_cast<user_t *>(ws->getUserData());
				_log.info("Session {} is behind data, closing", addr);
				if (user->position == first)
					ws->close();
			}
		} while (true);

		for (auto &[a, ws] : _sessions) {
			auto user = static_cast<user_t *>(ws->getUserData());
			if (user->position == last)
				writeable(ws, user);
		}
		return 0;
	}

	int _post_control(Response * resp, const tll_msg_t *msg, int flags)
	{
		if (msg->msgid != http_scheme::disconnect<tll_msg_t>::meta_id())
			return 0;
		resp->end();
		_disconnected(nullptr, msg->addr);
		return 0;
	}

	int _post(const tll_msg_t *msg, int flags)
	{
		if (msg->type == TLL_MESSAGE_DATA)
			return _post_data(nullptr, msg, flags);
		return Parent::_post(msg, flags);
	}

	int _connected(Response * ws, std::string_view url, tll_addr_t * addr)
	{
		auto user = static_cast<user_t *>(ws->getUserData());
		user->position = _ring.end();

		return Parent::_connected(ws, url, addr);
	}

	void writeable(Response * ws, user_t * user)
	{
		if (ws->getBufferedAmount() > 0)
			return;
		if (user->position == _ring.end())
			return;

		_log.debug("Post data to {}", user->addr.u64);
		auto data = std::string_view((const char *) user->position->data(), user->position->size);
		user->position++;

		ws->send(data);
	}
};

int WSServer::_init(const Channel::Url &url, Channel * master)
{
	//if (!url.host().size())
	//	return _log.fail(EINVAL, "No path to database");

	if (_instance)
		return _log.fail(EINVAL, "Only one UWS server per thread, blocked by existing: '{}'", _instance->name);

	_scheme_control.reset(context().scheme_load(http_scheme::scheme_string));
	if (!_scheme_control.get())
		return _log.fail(EINVAL, "Failed to load control scheme");

	auto host = url.host();
	auto sep = host.find_last_of(':');
	if (sep == host.npos)
		return this->_log.fail(EINVAL, "Invalid host:port pair: {}", host);
	auto p = conv::to_any<unsigned short>(host.substr(sep + 1));
	if (!p)
		return this->_log.fail(EINVAL, "Invalid port '{}': {}", host.substr(sep + 1), p.error());

	_port = *p;
	_host = host.substr(0, sep);

	auto reader = channel_props_reader(url);
	/*
	_table = reader.getT<std::string>("table");
	if ((internal.caps & (caps::Input | caps::Output)) == caps::Input)
		_autoclose = reader.getT("autoclose", false);
		*/
	if (!reader)
		return _log.fail(EINVAL, "Invalid url: {}", reader.error());

	_instance = this;
	return 0;
}

int WSServer::_open(const ConstConfig &s)
{
#ifdef __linux__
	auto _timerfd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
	if (_timerfd == -1)
		return _log.fail(EINVAL, "Failed to create timer fd: {}", strerror(errno));

	struct itimerspec its = {};
	its.it_interval = { 0, 100000000 };
	its.it_value = { 0, 100000000 };
	if (timerfd_settime(_timerfd, 0, &its, nullptr))
		return _log.fail(EINVAL, "Failed to rearm timerfd: {}", strerror(errno));

	if (uv_loop_init(&_uv_loop))
		return _log.fail(EINVAL, "Failed to init libuv event loop");
	auto fd = uv_backend_fd(&_uv_loop);

	_app_loop = uWS::Loop::get(&_uv_loop);

	uv_poll_init(&_uv_loop, &_uv_timer, _timerfd);
	uv_poll_start(&_uv_timer, UV_READABLE, [](uv_poll_t * handle, int status, int event) {
		int64_t buf;
		auto r = read(handle->io_watcher.fd, &buf, sizeof(buf));
		(void) r;
	});

	uv_run(&_uv_loop, UV_RUN_NOWAIT);

	if (fd != -1) {
		_update_fd(fd);
		_update_dcaps(dcaps::CPOLLIN);
	}
#endif

	_app.reset(new uWS::App());
	uWS::App::WebSocketBehavior wsopt = {};

	wsopt.compression = uWS::SHARED_COMPRESSOR;
	wsopt.maxPayloadLength = 16 * 1024;
	wsopt.idleTimeout = 10;
	wsopt.maxBackpressure = 1 * 1024 * 1024;

	/* Handlers */
	wsopt.upgrade = [this](auto *res, auto *req, auto *context) { return this->_ws_upgrade(res, req, context); };
	wsopt.open = [this](auto *ws) { this->_ws_open(ws); };
	wsopt.message = [this](auto *ws, std::string_view message, uWS::OpCode opCode) { this->_ws_message(ws, message, opCode); };
	wsopt.drain = [this](auto *ws) { this->_ws_drain(ws); };
	wsopt.close = [this](auto *ws, int code, std::string_view message) { this->_ws_close(ws, code, message); };

	_app->get("/*", [this](auto *res, auto *req) { this->_http<Method::GET>(res, req); })
		.post("/*", [this](auto *res, auto *req) { this->_http<Method::POST>(res, req); })
		.put("/*", [this](auto *res, auto *req) { this->_http<Method::PUT>(res, req); })
		.head("/*", [this](auto *res, auto *req) { this->_http<Method::HEAD>(res, req); })
		.options("/*", [this](auto *res, auto *req) { this->_http<Method::OPTIONS>(res, req); })
		.ws<user_t>("/*", std::move(wsopt))
		.listen(_port, [this](auto *token) {
			this->_app_socket = token;
			if (token) {
				this->_log.info("Serving");
			}
		});

	return 0;
}

int WSServer::_close()
{
	this->_update_fd(-1);

	if (_timerfd != -1) {
		::close(_timerfd);
		_timerfd = -1;
	}

	uv_close((uv_handle_t *) &_uv_timer, nullptr);

	_log.debug("Close UV loop");

	if (_app_socket) {
		us_listen_socket_close(0, _app_socket);
		_app_socket = nullptr;
	}

	//uv_loop_close(&_uv_loop);
	for (auto i = 0u; i < 100 && uv_loop_close(&_uv_loop) == UV_EBUSY; i++) {
		if (_app_loop) _app_loop->integrate();
		uv_run(&_uv_loop, UV_RUN_ONCE);
	}
	_app.reset();
	for (auto i = 0u; i < 100 && uv_loop_close(&_uv_loop) == UV_EBUSY; i++) {
		if (_app_loop) _app_loop->integrate();
		uv_run(&_uv_loop, UV_RUN_ONCE);
	}
	_app_loop->free();
	for (auto i = 0u; i < 100 && uv_loop_close(&_uv_loop) == UV_EBUSY; i++) {
		uv_run(&_uv_loop, UV_RUN_ONCE);
	}

	return 0;
}

int WSServer::_process(long timeout, int flags)
{
	_app_loop->integrate();
	auto r = uv_run(&_uv_loop, UV_RUN_NOWAIT);
	if (r < 0)
		return _log.fail(EINVAL, "UV run failed: {}", r);
	return 0;
}

template <typename T, typename R>
int WSNode<T, R>::_init(const Channel::Url &url, Channel * master)
{
	if (!master)
		return this->_log.fail(EINVAL, "WS node channel needs master");

	_master = channel_cast<WSServer>(master);
	if (!_master)
		return this->_log.fail(EINVAL, "Master {} must be ws:// channel", master->name());

	this->_scheme_control.reset(tll_scheme_ref(_master->_scheme_control.get()));

	_prefix = url.host();
	if (!_prefix.size())
		_prefix = "/";
	else if (_prefix[0] != '/')
		_prefix = "/" + _prefix;
	return 0;
}

template <typename T, typename R>
int WSNode<T, R>::_open(const ConstConfig &props)
{
	if (_master->node_add(_prefix, static_cast<T *>(this)))
		return this->_log.fail(EINVAL, "Failed to register node");
	_addr = {};
	return 0;
}

template <typename T, typename R>
int WSNode<T, R>::_close()
{
	for (auto &[a, r] : _sessions)
		r->close();
	_sessions.clear();
	_master->node_remove(_prefix, static_cast<T *>(this));
	return 0;
}

template <typename T, typename R>
int WSNode<T, R>::_connected(R * resp, std::string_view uri, tll_addr_t * addr)
{
	std::vector<unsigned char> buf;
	auto data = tll::scheme::make_binder<http_scheme::connect>(buf);
	buf.resize(data.meta_size());

	data.set_path(uri);

	*addr = _next_addr();
	_sessions.insert(std::make_pair(addr->u64, resp));

	tll_msg_t msg = {};
	msg.type = TLL_MESSAGE_CONTROL;
	msg.msgid = data.meta_id();
	msg.addr = *addr;
	msg.data = buf.data();
	msg.size = buf.size();
	this->_callback(&msg);
	return 0;
}

template <typename T, typename R>
int WSNode<T, R>::_disconnected(R * resp, tll_addr_t addr)
{
	if (this->state() != tll::state::Closing) {
		auto it = _sessions.find(addr.u64);
		if (it != _sessions.end())
			_sessions.erase(it);
	}

	std::vector<char> buf;
	auto data = tll::scheme::make_binder<http_scheme::disconnect>(buf);
	buf.resize(data.meta_size());

	tll_msg_t msg = {};
	msg.type = TLL_MESSAGE_CONTROL;
	msg.addr = addr;
	msg.msgid = data.meta_id();
	msg.data = buf.data();
	msg.size = buf.size();
	this->_callback(&msg);

	//user->pending = {};

	if (resp)
		resp->close();
	return 0;
}

template <WSServer::Method M>
void WSServer::_http(uWS::HttpResponse<false> * resp, uWS::HttpRequest *req)
{
	auto uri = req->getUrl();
	_log.debug("Requested {}", uri);
	auto it = _nodes.find(uri);
	if (it == _nodes.end()) {
		_log.debug("Requested url not found: '{}'", uri);
		resp->writeStatus("404 Not Found");
		return resp->end("Requested url not found");
	}

	if (std::holds_alternative<WSWS *>(it->second)) {
		_log.debug("HTTP request to WS endpoint {}", uri);
		resp->writeStatus("400 Bad Request");
		return resp->end("WebSocket node");
	}

	auto channel = std::get<WSHTTP *>(it->second);
	tll_addr_t addr = {};
	channel->_connected(resp, uri, &addr);

	auto h = req->getHeader("content-length");
	if (h.size()) {
		_log.debug("Content-Length: {}", h);
	}

	resp->onAborted([channel, addr]() { channel->_disconnected(nullptr, addr); });
	resp->onData([channel, addr](std::string_view data, bool last) {
		tll_msg_t msg = {};
		msg.type = TLL_MESSAGE_DATA;
		msg.addr = addr;
		msg.data = data.data();
		msg.size = data.size();
		channel->_callback_data(&msg);
	});
}

void WSServer::_ws_upgrade(uWS::HttpResponse<false> * resp, uWS::HttpRequest *req, us_socket_context_t *context)
{
	auto uri = req->getUrl();
	_log.debug("Requested {}", uri);
	auto it = _nodes.find(uri);
	if (it == _nodes.end()) {
		_log.debug("Requested url not found: '{}'", uri);
		resp->writeStatus("404 Not Found");
		return resp->end("Requested url not found");
	}

	std::variant<WSWS *, WSPub *> channel;

	if (std::holds_alternative<WSHTTP *>(it->second)) {
		_log.debug("WS request to HTTP endpoint {}", uri);
		resp->writeStatus("400 Bad Request");
		return resp->end("HTTP node");
	} else if (std::holds_alternative<WSWS *>(it->second)) {
		channel = std::get<WSWS *>(it->second);
	} else if (std::holds_alternative<WSPub *>(it->second)) {
		channel = std::get<WSPub *>(it->second);
	}

	resp->template upgrade<user_t>({ channel }
		, req->getHeader("sec-websocket-key")
		, req->getHeader("sec-websocket-protocol")
		, req->getHeader("sec-websocket-extensions")
		, context
		);
}

void WSServer::_ws_open(uWS::WebSocket<false, true> *ws)
{
	auto user = static_cast<user_t *>(ws->getUserData());
	std::visit([&ws, &user](auto && c) { c->_connected(ws, "", &user->addr); }, user->channel);
}

void WSServer::_ws_message(uWS::WebSocket<false, true> *ws, std::string_view message, uWS::OpCode)
{
	auto user = static_cast<user_t *>(ws->getUserData());

	if (!std::holds_alternative<WSWS *>(user->channel))
		return;
	tll_msg_t msg = {};
	msg.type = TLL_MESSAGE_DATA;
	msg.addr = user->addr;
	msg.data = message.data();
	msg.size = message.size();
	std::get<WSWS *>(user->channel)->_callback_data(&msg);
}

void WSServer::_ws_drain(uWS::WebSocket<false, true> *ws)
{
	auto user = static_cast<user_t *>(ws->getUserData());

	if (std::holds_alternative<WSPub *>(user->channel))
		return std::get<WSPub *>(user->channel)->writeable(ws, user);
}

void WSServer::_ws_close(uWS::WebSocket<false, true> *ws, int code, std::string_view message)
{
	auto user = static_cast<user_t *>(ws->getUserData());
	std::visit([&user](auto && c) { c->_disconnected(nullptr, user->addr); }, user->channel);
}

TLL_DEFINE_IMPL(WSServer);
TLL_DEFINE_IMPL(WSHTTP);
TLL_DEFINE_IMPL(WSWS);
TLL_DEFINE_IMPL(WSPub);

TLL_DEFINE_MODULE(WSServer, WSHTTP, WSWS, WSPub);
