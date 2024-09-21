#pragma once

#include <tll/scheme/binder.h>
#include <tll/util/conv.h>

namespace http_scheme {

static constexpr std::string_view scheme_string = R"(yamls+gz://eJyNUMtOwzAQvPcr9mYJJRItJUBukWNIJXAq6n5AlLjEUuNUsYsEVf6dNQl9V/Q2uzvaefigs0qGQMgAQOp1ZUIEAORN2rIuSAgb+7VCgtL20ftl4IrQlHNGBZ4DD0jMXplgONzj8MLceogoYVGMcIQwnYpJymc4PeA0jQRNED85nM4c/87BuUNjROI9ou4fKpI5j9nzhDP36rZtB/6f40RmhWyc74WSy6I37sOmv5fd3YMuADG2UfqDtEe0z2y5lmdYWx1aay1z64RUgckuClZdZdtXfYXHgnld7Olhr8PghGPU9yEnGJ9wVpkt/03XlWB2vJu+tr2AsTL5YcbRxYzXmJdNU5/r/QcaZa0D)";

enum class Method: int8_t
{
	UNDEFINED = 0,
	GET = 1,
	HEAD = 2,
	POST = 3,
	PUT = 4,
	DELETE = 5,
	CONNECT = 6,
	OPTIONS = 7,
	TRACE = 8,
	PATCH = 9,
};

struct Header
{
	static constexpr size_t meta_size() { return 16; }
	static constexpr std::string_view meta_name() { return "Header"; }

	template <typename Buf>
	struct binder_type : public tll::scheme::Binder<Buf>
	{
		using tll::scheme::Binder<Buf>::Binder;

		static constexpr auto meta_size() { return Header::meta_size(); }
		static constexpr auto meta_name() { return Header::meta_name(); }
		void view_resize() { this->_view_resize(meta_size()); }

		std::string_view get_header() const { return this->template _get_string<tll_scheme_offset_ptr_t>(0); }
		void set_header(std::string_view v) { return this->template _set_string<tll_scheme_offset_ptr_t>(0, v); }

		std::string_view get_value() const { return this->template _get_string<tll_scheme_offset_ptr_t>(8); }
		void set_value(std::string_view v) { return this->template _set_string<tll_scheme_offset_ptr_t>(8, v); }
	};

	template <typename Buf>
	static binder_type<Buf> bind(Buf &buf, size_t offset = 0) { return binder_type<Buf>(tll::make_view(buf).view(offset)); }

	template <typename Buf>
	static binder_type<Buf> bind_reset(Buf &buf) { return tll::scheme::make_binder_reset<binder_type, Buf>(buf); }
};

struct Connect
{
	static constexpr size_t meta_size() { return 27; }
	static constexpr std::string_view meta_name() { return "Connect"; }
	static constexpr int meta_id() { return 1; }

	template <typename Buf>
	struct binder_type : public tll::scheme::Binder<Buf>
	{
		using tll::scheme::Binder<Buf>::Binder;

		static constexpr auto meta_size() { return Connect::meta_size(); }
		static constexpr auto meta_name() { return Connect::meta_name(); }
		static constexpr auto meta_id() { return Connect::meta_id(); }
		void view_resize() { this->_view_resize(meta_size()); }

		using type_method = Method;
		type_method get_method() const { return this->template _get_scalar<type_method>(0); }
		void set_method(type_method v) { return this->template _set_scalar<type_method>(0, v); }

		using type_code = int16_t;
		type_code get_code() const { return this->template _get_scalar<type_code>(1); }
		void set_code(type_code v) { return this->template _set_scalar<type_code>(1, v); }

		using type_size = int64_t;
		type_size get_size() const { return this->template _get_scalar<type_size>(3); }
		void set_size(type_size v) { return this->template _set_scalar<type_size>(3, v); }

		std::string_view get_path() const { return this->template _get_string<tll_scheme_offset_ptr_t>(11); }
		void set_path(std::string_view v) { return this->template _set_string<tll_scheme_offset_ptr_t>(11, v); }

		using type_headers = tll::scheme::binder::List<Buf, Header::binder_type<Buf>, tll_scheme_offset_ptr_t>;
		const type_headers get_headers() const { return this->template _get_binder<type_headers>(19); }
		type_headers get_headers() { return this->template _get_binder<type_headers>(19); }
	};

	template <typename Buf>
	static binder_type<Buf> bind(Buf &buf, size_t offset = 0) { return binder_type<Buf>(tll::make_view(buf).view(offset)); }

	template <typename Buf>
	static binder_type<Buf> bind_reset(Buf &buf) { return tll::scheme::make_binder_reset<binder_type, Buf>(buf); }
};

struct Disconnect
{
	static constexpr size_t meta_size() { return 10; }
	static constexpr std::string_view meta_name() { return "Disconnect"; }
	static constexpr int meta_id() { return 2; }

	template <typename Buf>
	struct binder_type : public tll::scheme::Binder<Buf>
	{
		using tll::scheme::Binder<Buf>::Binder;

		static constexpr auto meta_size() { return Disconnect::meta_size(); }
		static constexpr auto meta_name() { return Disconnect::meta_name(); }
		static constexpr auto meta_id() { return Disconnect::meta_id(); }
		void view_resize() { this->_view_resize(meta_size()); }

		using type_code = int16_t;
		type_code get_code() const { return this->template _get_scalar<type_code>(0); }
		void set_code(type_code v) { return this->template _set_scalar<type_code>(0, v); }

		std::string_view get_error() const { return this->template _get_string<tll_scheme_offset_ptr_t>(2); }
		void set_error(std::string_view v) { return this->template _set_string<tll_scheme_offset_ptr_t>(2, v); }
	};

	template <typename Buf>
	static binder_type<Buf> bind(Buf &buf, size_t offset = 0) { return binder_type<Buf>(tll::make_view(buf).view(offset)); }

	template <typename Buf>
	static binder_type<Buf> bind_reset(Buf &buf) { return tll::scheme::make_binder_reset<binder_type, Buf>(buf); }
};

} // namespace http_scheme

template <>
struct tll::conv::dump<http_scheme::Method> : public to_string_from_string_buf<http_scheme::Method>
{
	template <typename Buf>
	static inline std::string_view to_string_buf(const http_scheme::Method &v, Buf &buf)
	{
		switch (v) {
		case http_scheme::Method::CONNECT: return "CONNECT";
		case http_scheme::Method::DELETE: return "DELETE";
		case http_scheme::Method::GET: return "GET";
		case http_scheme::Method::HEAD: return "HEAD";
		case http_scheme::Method::OPTIONS: return "OPTIONS";
		case http_scheme::Method::PATCH: return "PATCH";
		case http_scheme::Method::POST: return "POST";
		case http_scheme::Method::PUT: return "PUT";
		case http_scheme::Method::TRACE: return "TRACE";
		case http_scheme::Method::UNDEFINED: return "UNDEFINED";
		default: break;
		}
		return tll::conv::to_string_buf<int8_t, Buf>((int8_t) v, buf);
	}
};
