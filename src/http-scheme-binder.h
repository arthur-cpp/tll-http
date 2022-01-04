#pragma once

#include <tll/scheme/binder.h>

namespace http_scheme {

static constexpr std::string_view scheme_string =
    R"(yamls+gz://eJyNkF9rwjAUxd/9FPctIC2sznXSt9JmUxipzPg8SpPZgE2liYNN+t13szr/VGV7+93kcM89xwedVzICQgYAUm8rEyEAkErashZvlkSws58blChtJ96PBp9IkjFGE47fDx6QlL5QTnEY4/BM3fMd0pTGKWKAmM35LGMLnEKc5jFPpsgTx9nC6UcOl47ukfhrnLh9j8hLltKnGaNulR+07cD/PbqUuZCNO/1dybXY3+7D7vzfgy4BMbZRekXanuwjX2/lFdXBp6i1loV1RkpgnpuGXWvHVYcW+5ZFLU4csdogvNAY9XWuCccXmk1uyz/zdTWYo264L+YkolCml3J0M+V/jpdNU19r/hsDAq+O)";

enum class method_t : int8_t
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

template <typename Buf>
struct header : public tll::scheme::Binder<Buf>
{
	using tll::scheme::Binder<Buf>::Binder;

	static constexpr size_t meta_size() { return 16; }

	std::string_view get_header() const { return this->template _get_string<tll_scheme_offset_ptr_t>(0); }
	void set_header(std::string_view v) { return this->template _set_string<tll_scheme_offset_ptr_t>(0, v); }

	std::string_view get_value() const { return this->template _get_string<tll_scheme_offset_ptr_t>(8); }
	void set_value(std::string_view v) { return this->template _set_string<tll_scheme_offset_ptr_t>(8, v); }
};

template <typename Buf>
struct connect : public tll::scheme::Binder<Buf>
{
	using tll::scheme::Binder<Buf>::Binder;

	static constexpr size_t meta_size() { return 27; }
	static constexpr int meta_id() { return 1; }

	method_t get_method() const { return this->template _get_scalar<method_t>(0); };
	void set_method(method_t v) { return this->template _set_scalar<method_t>(0, v); };

	int16_t get_code() const { return this->template _get_scalar<int16_t>(1); };
	void set_code(int16_t v) { return this->template _set_scalar<int16_t>(1, v); };

	int64_t get_size() const { return this->template _get_scalar<int64_t>(3); };
	void set_size(int64_t v) { return this->template _set_scalar<int64_t>(3, v); };

	std::string_view get_path() const { return this->template _get_string<tll_scheme_offset_ptr_t>(11); }
	void set_path(std::string_view v) { return this->template _set_string<tll_scheme_offset_ptr_t>(11, v); }

	using type_headers = tll::scheme::binder::List<Buf, header<Buf>, tll_scheme_offset_ptr_t>;
	const type_headers get_headers() const { return this->template _get_binder<type_headers>(19); }
	type_headers get_headers() { return this->template _get_binder<type_headers>(19); }
};

template <typename Buf>
struct disconnect : public tll::scheme::Binder<Buf>
{
	using tll::scheme::Binder<Buf>::Binder;

	static constexpr size_t meta_size() { return 10; }
	static constexpr int meta_id() { return 2; }

	int16_t get_code() const { return this->template _get_scalar<int16_t>(0); };
	void set_code(int16_t v) { return this->template _set_scalar<int16_t>(0, v); };

	std::string_view get_error() const { return this->template _get_string<tll_scheme_offset_ptr_t>(2); }
	void set_error(std::string_view v) { return this->template _set_string<tll_scheme_offset_ptr_t>(2, v); }
};

} // namespace http_scheme
