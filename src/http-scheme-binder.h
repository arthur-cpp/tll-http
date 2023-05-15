#pragma once

#include <tll/scheme/binder.h>
#include <tll/util/conv.h>

namespace http_scheme {

static constexpr std::string_view scheme_string = R"(yamls+gz://eJyNkN1OwzAMhe/3FL6LhFqJjlGm3lVpoJMgnVj2AFUbaKQ1nZoMCaa+Ow4t+5/g7rN9ZPscH3ReywgIGQFIvalNhABAXqStmpJEsLWfaxQobafejwJbhGacMypwfO8BSdgzEwyLCRZPzLVvkVIWJ4gBYjYXs4wvsAqxmseCpshTx9nC6ccOl47ukMRrTN2+B+QlT9jjjDO3yg+6buT/vpzKvJSte/xNyVU5fO7DdphX/dyD3gExtlX6nXQnso98tZEXVLs7tNFaFtYdUiX6uXqw7jPbrRoyPD1YNOXBPQw2CM80Rn0da8LJmWad2+pPd30IZq+7GWI7MJgoUxx7HF/1+J/nZds2l3L/BoM7rSg=)";

enum class Method: int8_t
{
	UNDEFINED = -1,
	GET = 0,
	HEAD = 1,
	POST = 2,
	PUT = 3,
	DELETE = 4,
	CONNECT = 5,
	OPTIONS = 6,
	TRACE = 7,
	PATCH = 8,
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
