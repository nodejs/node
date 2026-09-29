// Slightly modified version of fmt lib's format.cc source file.
// Copyright (c) 2012 - 2016, Victor Zverovich
// All rights reserved.

#ifndef SPDLOG_COMPILED_LIB
#error Please define SPDLOG_COMPILED_LIB to compile this file.
#endif

#if !defined(SPDLOG_FMT_EXTERNAL) && !defined(SPDLOG_USE_STD_FORMAT)

#include <spdlog/fmt/bundled/format-inl.h>

FMT_BEGIN_NAMESPACE

#if FMT_USE_LOCALE
template FMT_API locale_ref::locale_ref(const std::locale& loc);  // DEPRECATED!
template FMT_API auto locale_ref::get<std::locale>() const -> std::locale;
#endif

namespace detail {

template FMT_API auto dragonbox::to_decimal(float x) noexcept
    -> dragonbox::decimal_fp<float>;
template FMT_API auto dragonbox::to_decimal(double x) noexcept
    -> dragonbox::decimal_fp<double>;

// Explicit instantiations for char.

template FMT_API auto thousands_sep_impl(locale_ref)
    -> thousands_sep_result<char>;
template FMT_API auto decimal_point_impl(locale_ref) -> char;

// DEPRECATED!
template FMT_API void buffer<char>::append(const char*, const char*);

// Explicit instantiations for wchar_t.

template FMT_API auto thousands_sep_impl(locale_ref)
    -> thousands_sep_result<wchar_t>;
template FMT_API auto decimal_point_impl(locale_ref) -> wchar_t;

// DEPRECATED!
template FMT_API void buffer<wchar_t>::append(const wchar_t*, const wchar_t*);

}  // namespace detail
FMT_END_NAMESPACE

#endif  // !SPDLOG_FMT_EXTERNAL
