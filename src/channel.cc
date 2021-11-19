/*
 * Copyright (c) 2021 Pavel Shramov <shramov@mexmat.net>
 *
 * tll is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#include <libwebsockets.h>

#include <variant>

#include "tll/channel/base.h"
#include "tll/util/ownedmsg.h"
#include "names.h"
#include "lws_scheme.h"
#include "ev-backend.h"

#ifdef __linux__
#include <sys/timerfd.h>
#include <unistd.h>
#endif

#if 0
#include <uv.h>
#else
#include <ev.h>
#endif

namespace {
std::optional<std::string_view> tll_lws_http_get_uri(lws * wsi)
{
	char * curi;
	int curi_len;
	if (lws_http_get_uri_and_method(wsi, &curi, &curi_len) < 0)
		return std::nullopt;
	return std::string_view(curi, (unsigned) curi_len);
}

int tll_lws_drop(lws * wsi, http_status s, int r = 1)
{
	if (r > 0)
		lws_return_http_status(wsi, s, NULL);
	return r;
}
}

using namespace tll;

class WSHTTP;
class WSSSE;
class WSWS;

class WSServer : public tll::channel::Base<WSServer>
{
	using node_ptr_t = std::variant<WSHTTP *, WSSSE *, WSWS *>;
 	std::map<std::string_view, node_ptr_t, std::less<>> _nodes;

	std::vector<lws_protocols> _protocols;
	lws_context_creation_info _info = {};
	lws_context * _lws = nullptr;
	void * _loop_ptr[1] = {};

	int _timerfd = -1;

#if 0
	uv_loop_t _uv_loop = {};
	//uv_timer_t _uv_timer = {};
	uv_poll_t _uv_timer = {};

#else
	struct ev_loop * _ev_loop = nullptr;
	struct ev_io _ev_timer = {};
#endif

 public:
	struct user_t {
		node_ptr_t channel = {};
		tll_addr_t addr;
		unsigned short close = 0;
		tll::util::OwnedMessage pending;
	};

	static constexpr std::string_view channel_protocol() { return "ws"; }

	int _init(const tll::Channel::Url &, tll::Channel *master);
	int _open(const tll::PropsView &);
	int _close();
	void _destroy();

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
	int _lws_callback(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len);

	static int _lws_callback_s(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len)
	{
		if (reason == LWS_CALLBACK_GET_THREAD_ID)
			return 0;
		auto self = static_cast<WSServer *>(lws_get_protocol(wsi)->user);
		return self->_lws_callback(wsi, reason, user, in, len);
	}
};

template <typename T>
class WSNode : public tll::channel::Base<T>
{
 protected:
 	WSServer * _master = nullptr;
	using Base = tll::channel::Base<T>;

	std::string _prefix;

	std::map<uint64_t, lws *> _sessions;
	tll_addr_t _addr;

	using user_t = WSServer::user_t;

 public:
	static constexpr std::string_view param_prefix() { return "ws"; }
	static constexpr auto process_policy() { return Base::ProcessPolicy::Never; }

	int _init(const tll::Channel::Url &, tll::Channel *master);
	int _open(const tll::PropsView &);
	int _close();

	int _post_data(const tll_msg_t *msg, int flags);
	int _post_control(const tll_msg_t *msg, int flags);

	int _post(const tll_msg_t *msg, int flags)
	{
		if (msg->type == TLL_MESSAGE_DATA)
			return _post_data(msg, flags);
		else if (msg->type == TLL_MESSAGE_CONTROL)
			return _post_control(msg, flags);
		return 0;
	}

	tll::util::OwnedMessage copy(const tll_msg_t *msg) { return msg; }

 protected:
	int _connected(lws * wsi, user_t * user);
	int _disconnected(lws * wsi, user_t * user);

	tll_addr_t _next_addr()
	{
		return { ++_addr.u64 }; // TODO: Check for duplicates
	}
};

class WSHTTP : public WSNode<WSHTTP>
{
 public:
	static constexpr std::string_view channel_protocol() { return "ws+http"; }

	int lws_callback(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len);
};

class WSSSE : public WSNode<WSSSE>
{
 public:
	static constexpr std::string_view channel_protocol() { return "ws+sse"; }

	int lws_callback(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len);

	tll::util::OwnedMessage copy(const tll_msg_t *msg)
	{
		tll::util::OwnedMessage omsg = {};
		tll_msg_copy_info(omsg, msg);
		omsg.size = msg->size + 6 + 2;
		char * buf = new char[omsg.size]; 
		omsg.data = buf;
		memcpy(buf, "data: ", 6);
		memcpy(buf + 6, msg->data, msg->size);
		buf[6 + msg->size] = '\n';
		buf[6 + msg->size + 1] = '\n';
		return omsg;
	}
};

class WSWS : public WSNode<WSWS>
{
 public:
	static constexpr std::string_view channel_protocol() { return "ws+ws"; }

	int lws_callback(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len);

	tll::util::OwnedMessage copy(const tll_msg_t *msg)
	{
		tll::util::OwnedMessage omsg = {};
		tll_msg_copy_info(omsg, msg);
		omsg.size = msg->size + LWS_PRE;
		omsg.data = new char[omsg.size];
		memcpy(((unsigned char *) omsg.data) + LWS_PRE, msg->data, msg->size);
		return omsg;
	}
};

int WSServer::_init(const Channel::Url &url, Channel * master)
{
	//if (!url.host().size())
	//	return _log.fail(EINVAL, "No path to database");

	_scheme_control.reset(context().scheme_load(lws_scheme::scheme));
	if (!_scheme_control.get())
		return _log.fail(EINVAL, "Failed to load control scheme");

	_protocols.push_back(lws_protocols { "tll", _lws_callback_s, sizeof(user_t), 0, 0, this });
	_protocols.push_back(lws_protocols {});

	_info.protocols = _protocols.data();
	_info.port = 8080;
	_info.foreign_loops = _loop_ptr;
	//_info.options = LWS_SERVER_OPTION_HTTP_HEADERS_SECURITY_BEST_PRACTICES_ENFORCE;

	auto reader = channel_props_reader(url);
	/*
	_table = reader.getT<std::string>("table");
	if ((internal.caps & (caps::Input | caps::Output)) == caps::Input)
		_autoclose = reader.getT("autoclose", false);
		*/
	if (!reader)
		return _log.fail(EINVAL, "Invalid url: {}", reader.error());

	return 0;
}

namespace {
#if 0
void poll_cb_uv(uv_poll_t* handle, int status, int events)
{
	int64_t buf;
	auto r = read(handle->io_watcher.fd, &buf, sizeof(buf));
	(void) r;
}
#else
void poll_cb_ev(struct ev_loop *, ev_io *ev, int)
{
	int64_t buf;
	auto r = read(ev->fd, &buf, sizeof(buf));
	(void) r;
}
#endif
}


int WSServer::_open(const PropsView &s)
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
#if 0
	if (uv_loop_init(&_uv_loop))
		return _log.fail(EINVAL, "Failed to init libuv event loop");
	auto fd = uv_backend_fd(&_uv_loop);

	uv_poll_init(&_uv_loop, &_uv_timer, _timerfd);
	uv_poll_start(&_uv_timer, UV_READABLE, poll_cb_uv);

	uv_run(&_uv_loop, UV_RUN_NOWAIT);

	_loop_ptr[0] = &_uv_loop;
	_info.options |= LWS_SERVER_OPTION_LIBUV;
#else
	_ev_loop = ev_loop_new(EVFLAG_NOENV | EVFLAG_NOSIGMASK);
	if (!_ev_loop)
		return _log.fail(EINVAL, "Faield to ini libev event loop");

	ev_io_init(&_ev_timer, poll_cb_ev, _timerfd, EV_READ);
	ev_io_start(_ev_loop, &_ev_timer);

	ev_run(_ev_loop, EVRUN_NOWAIT);

	auto fd = tll_ev_backend_fd(_ev_loop);

	_loop_ptr[0] = _ev_loop;
	_info.options |= LWS_SERVER_OPTION_LIBEV;
#endif
#endif
	if (fd != -1) {
		_update_fd(fd);
		_update_dcaps(dcaps::CPOLLIN);
	}

	_lws = lws_create_context(&_info);
	if (!_lws)
		return _log.fail(EINVAL, "Failed to create LWS context");

	return 0;
}

int WSServer::_close()
{
	if (_lws) {
		lws_context_destroy(_lws);
		_lws = nullptr;
	}

	this->_update_fd(-1);

	if (_timerfd != -1) {
		::close(_timerfd);
		_timerfd = -1;
	}

#if 0
	uv_close((uv_handle_t *) &_uv_timer, nullptr);

	_log.debug("Close UV loop");

	uv_loop_close(&_uv_loop);
	for (auto i = 0u; i < 100000 && uv_loop_close(&_uv_loop) == UV_EBUSY; i++)
		uv_run(&_uv_loop, UV_RUN_ONCE);
#else
	if (_ev_loop) {
		ev_loop_destroy(_ev_loop);
		_ev_loop = nullptr;
	}
#endif

	return 0;
}

int WSHTTP::lws_callback(struct lws *wsi, enum lws_callback_reasons reason, void *_user, void *in, size_t len)
{
	auto user = static_cast<user_t *>(_user);

	_log.trace("Callback {}", lws_callback_name(reason));
	switch (reason) {
	case LWS_CALLBACK_HTTP:
		if (_connected(wsi, user))
			return tll_lws_drop(wsi, HTTP_STATUS_INTERNAL_SERVER_ERROR);
		break;

	case LWS_CALLBACK_HTTP_BODY:
		break;

	case LWS_CALLBACK_CLOSED_HTTP:
		_disconnected(wsi, user);
		break;

	case LWS_CALLBACK_HTTP_WRITEABLE: {
		if (user->close)
			return tll_lws_drop(wsi, (http_status) user->close);

		unsigned char buf[LWS_PRE + LWS_RECOMMENDED_MIN_HEADER_SPACE];
		auto start = &buf[LWS_PRE];
                auto p = start;
		auto end = &buf[sizeof(buf) - 1];

		if (lws_add_http_common_headers(wsi, HTTP_STATUS_OK, "application/octet-stream", user->pending.size, &p, end))
			return _log.fail(1, "Failed to set text/event-stream content type");

		if (lws_finalize_write_http_header(wsi, start, &p, end))
			return _log.fail(1, "Failed to finalize headers");

		if (lws_write(wsi, (unsigned char *) user->pending.data, user->pending.size, LWS_WRITE_HTTP) < (long) user->pending.size)
			return _log.fail(-1, "Failed to write data");

		user->pending = {};

		return -1;
	}

	default:
		break;
	}

	return 0;
}


int WSSSE::lws_callback(struct lws *wsi, enum lws_callback_reasons reason, void *_user, void *in, size_t len)
{
	auto user = static_cast<user_t *>(_user);

	_log.trace("Callback {}", lws_callback_name(reason));
	switch (reason) {
	case LWS_CALLBACK_HTTP: {
		if (_connected(wsi, user))
			return tll_lws_drop(wsi, HTTP_STATUS_INTERNAL_SERVER_ERROR);
		if (user->close)
			return tll_lws_drop(wsi, (http_status) user->close);

		unsigned char buf[LWS_PRE + LWS_RECOMMENDED_MIN_HEADER_SPACE];
		auto start = &buf[LWS_PRE];
                auto p = start;
		auto end = &buf[sizeof(buf) - 1];

		if (lws_add_http_common_headers(wsi, HTTP_STATUS_OK, "text/event-stream", LWS_ILLEGAL_HTTP_CONTENT_LEN, &p, end))
			return _log.fail(1, "Failed to set text/event-stream content type");

		if (lws_add_http_header_by_token(wsi, WSI_TOKEN_HTTP_ACCESS_CONTROL_ALLOW_ORIGIN, (unsigned char *) "*", 1, &p, end))
			return _log.fail(1, "Failed to add Access-Control-Allow-Origin header");

		if (lws_finalize_write_http_header(wsi, start, &p, end))
			return _log.fail(1, "Failed to finalize headers");

		lws_http_mark_sse(wsi);

		break;
	}

	case LWS_CALLBACK_HTTP_BODY:
		break;

	case LWS_CALLBACK_CLOSED_HTTP:
		_disconnected(wsi, user);
		break;

	case LWS_CALLBACK_HTTP_WRITEABLE:
		if (user->pending.size) {
			if (lws_write(wsi, (unsigned char *) user->pending.data, user->pending.size, LWS_WRITE_HTTP) < (long) user->pending.size)
				return _log.fail(-1, "Failed to write data");
			user->pending = {};
		}
		if (user->close)
			return tll_lws_drop(wsi, (http_status) user->close);
		break;

	default:
		break;
	}

	return 0;
}

int WSWS::lws_callback(struct lws *wsi, enum lws_callback_reasons reason, void *_user, void *in, size_t len)
{
	auto user = static_cast<user_t *>(_user);

	_log.trace("Callback {}", lws_callback_name(reason));
	switch (reason) {
	case LWS_CALLBACK_ESTABLISHED:
		if (_connected(wsi, user))
			return tll_lws_drop(wsi, HTTP_STATUS_INTERNAL_SERVER_ERROR);
		break;

	case LWS_CALLBACK_CLOSED:
		_disconnected(wsi, user);
		break;

	case LWS_CALLBACK_SERVER_WRITEABLE:
		if (user->pending.size) {
			const long size = user->pending.size - LWS_PRE;
			if (lws_write(wsi, ((unsigned char *) user->pending.data) + LWS_PRE, size, LWS_WRITE_TEXT) < size)
				return _log.fail(-1, "Failed to write data");
			user->pending = {};
		}
		if (user->close)
			return tll_lws_drop(wsi, (http_status) user->close, -1);
		break;

	case LWS_CALLBACK_RECEIVE: {
		tll_msg_t msg = {};
		msg.type = TLL_MESSAGE_DATA;
		msg.addr = user->addr;
		msg.data = in;
		msg.size = len;
		_callback_data(&msg);
		break;
	}

	default:
		break;
	}

	return 0;
}

int WSServer::_lws_callback(struct lws *wsi, enum lws_callback_reasons reason, void *_user, void *in, size_t len)
{
	_log.debug("Callback {}", lws_callback_name(reason));
	auto user = static_cast<user_t *>(_user);

	switch (reason) {
	case LWS_CALLBACK_FILTER_HTTP_CONNECTION: {
		std::string_view uri = { (const char *) in, len };
		auto it = _nodes.find(uri);
		if (it == _nodes.end()) {
			_log.debug("No endpoint at {}", uri);
			return tll_lws_drop(wsi, HTTP_STATUS_NOT_FOUND);
		}

		if (std::holds_alternative<WSWS *>(it->second)) {
			_log.debug("HTTP request to WS endpoint {}", uri);
			return tll_lws_drop(wsi, HTTP_STATUS_BAD_REQUEST);
		}

		return 0;
	}

	case LWS_CALLBACK_HTTP_CONFIRM_UPGRADE: {
		auto uri = tll_lws_http_get_uri(wsi);
		if (!uri)
			return _log.fail(-1, "Failed to get request URI");

		auto it = _nodes.find(*uri);
		if (it == _nodes.end()) {
			_log.debug("No endpoint at {}", *uri);
			return tll_lws_drop(wsi, HTTP_STATUS_NOT_FOUND);
		}
		if (!std::holds_alternative<WSWS *>(it->second)) {
			_log.debug("WS request to HTTP endpoint {}", *uri);
			return tll_lws_drop(wsi, HTTP_STATUS_BAD_REQUEST);
		}

		return 0;
	}

	// User data is reset on lws_bind_protocol so it's impossible to bind endpoint before this callbacks
	case LWS_CALLBACK_HTTP_BIND_PROTOCOL: {
		auto uri = tll_lws_http_get_uri(wsi);
		if (!uri)
			return _log.fail(-1, "Failed to get request URI");

		_log.info("New request to {}", *uri);
		auto it = _nodes.find(*uri);
		if (it == _nodes.end())
			return tll_lws_drop(wsi, HTTP_STATUS_NOT_FOUND);

		user->channel = it->second;
		_log.debug("Bind {} to {}", *uri, std::visit([](auto && c) { return std::string_view(c->name); }, user->channel));
		return 0;
	}

	case LWS_CALLBACK_HTTP_DROP_PROTOCOL:
		return 0;
	default:
		break;
	}

	if (!user)
		return 0;

	return std::visit([wsi, reason, user, in, len](auto && channel) {
		if (channel == nullptr)
			return 0;
		return channel->lws_callback(wsi, reason, user, in, len);
	}, user->channel);
}

int WSServer::_process(long timeout, int flags)
{
	//auto r = lws_service(_lws, -1);
#if 0
	auto r = uv_run(&_uv_loop, UV_RUN_NOWAIT);
#else
	auto r = ev_run(_ev_loop, EVRUN_NOWAIT);
#endif
	if (r < 0)
		return _log.fail(EINVAL, "LWS process failed: {}", r);
	return 0;
}

template <typename T>
int WSNode<T>::_init(const Channel::Url &url, Channel * master)
{
	if (!master)
		return this->_log.fail(EINVAL, "LWS node channel needs master");

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

template <typename T>
int WSNode<T>::_open(const PropsView &props)
{
	if (_master->node_add(_prefix, static_cast<T *>(this)))
		return this->_log.fail(EINVAL, "Failed to register node");
	_addr = {};
	return 0;
}

template <typename T>
int WSNode<T>::_close()
{
	_master->node_remove(_prefix, static_cast<T *>(this));
	_sessions.clear();
	return 0;
}

template <typename T>
int WSNode<T>::_post_control(const tll_msg_t *msg, int flags)
{
	if (msg->msgid != lws_scheme::disconnect::id)
		return 0;
	auto it = _sessions.find(msg->addr.u64);
	if (it == _sessions.end())
		return this->_log.fail(ENOENT, "Failed to disconnect: session 0x{:x} not found", msg->addr.u64);
	auto user = static_cast<user_t *>(lws_wsi_user(it->second));
	user->close = 200;
	lws_callback_on_writable(it->second);
	return 0;
}

template <typename T>
int WSNode<T>::_post_data(const tll_msg_t *msg, int flags)
{
	auto it = _sessions.find(msg->addr.u64);
	if (it == _sessions.end())
		return this->_log.fail(ENOENT, "Failed to post: session 0x{:x} not found", msg->addr.u64);
	auto user = static_cast<user_t *>(lws_wsi_user(it->second));

	user->pending = std::move(static_cast<T *>(this)->copy(msg));
	lws_callback_on_writable(it->second);
	return 0;
}

template <typename T>
int WSNode<T>::_connected(lws * wsi, WSServer::user_t * user)
{
	auto uri = tll_lws_http_get_uri(wsi);
	if (!uri)
		return this->_log.fail(EINVAL, "Failed to get request URI");

	std::vector<unsigned char> buf;
	buf.resize(sizeof(lws_scheme::connect) + uri->size() + 1);
	auto data = (lws_scheme::connect *) buf.data();

	data->path.entity = 1;
	data->path.size = uri->size() + 1;
	data->path.offset = sizeof(data->path);
	memcpy(data + 1, uri->data(), uri->size());

	user->addr = _next_addr();
	_sessions.insert(std::make_pair(user->addr.u64, wsi));

	tll_msg_t msg = {};
	msg.type = TLL_MESSAGE_CONTROL;
	msg.msgid = lws_scheme::connect::id;
	msg.addr = user->addr;
	msg.data = buf.data();
	msg.size = buf.size();
	this->_callback(&msg);
	return 0;
}

template <typename T>
int WSNode<T>::_disconnected(lws * wsi, WSServer::user_t * user)
{
	auto it = _sessions.find(user->addr.u64);
	if (it != _sessions.end())
		_sessions.erase(it);

	lws_scheme::disconnect data = {};
	tll_msg_t msg = {};
	msg.type = TLL_MESSAGE_CONTROL;
	msg.msgid = lws_scheme::disconnect::id;
	msg.addr = user->addr;
	msg.data = &data;
	msg.size = sizeof(data);
	this->_callback(&msg);

	user->pending = {};
	return 0;
}

TLL_DEFINE_IMPL(WSServer);
TLL_DEFINE_IMPL(WSHTTP);
TLL_DEFINE_IMPL(WSSSE);
TLL_DEFINE_IMPL(WSWS);

TLL_DEFINE_MODULE(WSServer, WSHTTP, WSSSE, WSWS);
