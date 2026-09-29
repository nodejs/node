//
// Copyright(c) 2016 Gabi Melman.
// Distributed under the MIT License (http://opensource.org/licenses/MIT)
//

#pragma once
//
// include bundled or external copy of fmtlib's std support (for formatting e.g.
// std::filesystem::path, std::thread::id, std::monostate, std::variant, ...)
//
#include <spdlog/tweakme.h>

#if !defined(SPDLOG_USE_STD_FORMAT)
#if !defined(SPDLOG_FMT_EXTERNAL)
#ifdef SPDLOG_HEADER_ONLY
#ifndef FMT_HEADER_ONLY
#define FMT_HEADER_ONLY
#endif
#endif
#include <spdlog/fmt/bundled/std.h>
#else
#include <fmt/std.h>
#endif
#endif
