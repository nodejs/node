// Copyright(c) 2015-present, Gabi Melman & spdlog contributors.
// Distributed under the MIT License (http://opensource.org/licenses/MIT)

#pragma once

#include <ctime>  // std::time_t
#include <spdlog/common.h>

namespace spdlog {
namespace details {
namespace os {

SPDLOG_API spdlog::log_clock::time_point now() SPDLOG_NOEXCEPT;

SPDLOG_API std::tm localtime(const std::time_t &time_tt) SPDLOG_NOEXCEPT;

SPDLOG_API std::tm localtime() SPDLOG_NOEXCEPT;

SPDLOG_API std::tm gmtime(const std::time_t &time_tt) SPDLOG_NOEXCEPT;

SPDLOG_API std::tm gmtime() SPDLOG_NOEXCEPT;

// eol definition
#if !defined(SPDLOG_EOL)
    #ifdef _WIN32
        #define SPDLOG_EOL "\r\n"
    #else
        #define SPDLOG_EOL "\n"
    #endif
#endif

SPDLOG_CONSTEXPR static const char *default_eol = SPDLOG_EOL;

// folder separator
#if !defined(SPDLOG_FOLDER_SEPS)
    #ifdef _WIN32
        #define SPDLOG_FOLDER_SEPS "\\/"
    #else
        #define SPDLOG_FOLDER_SEPS "/"
    #endif
#endif

SPDLOG_CONSTEXPR static const char folder_seps[] = SPDLOG_FOLDER_SEPS;
SPDLOG_CONSTEXPR static const filename_t::value_type folder_seps_filename[] =
    SPDLOG_FILENAME_T(SPDLOG_FOLDER_SEPS);

// fopen_s on non windows for writing
SPDLOG_API bool fopen_s(FILE **fp, const filename_t &filename, const filename_t &mode);

// Remove filename. return 0 on success
SPDLOG_API int remove(const filename_t &filename) SPDLOG_NOEXCEPT;

// Remove file if exists. return 0 on success
// Note: Non atomic (might return failure to delete if concurrently deleted by other process/thread)
SPDLOG_API int remove_if_exists(const filename_t &filename) SPDLOG_NOEXCEPT;

SPDLOG_API int rename(const filename_t &filename1, const filename_t &filename2) SPDLOG_NOEXCEPT;

// Return if file exists.
SPDLOG_API bool path_exists(const filename_t &filename) SPDLOG_NOEXCEPT;

// Return file size according to open FILE* object
SPDLOG_API size_t filesize(FILE *f);

// Return utc offset in minutes or throw spdlog_ex on failure
SPDLOG_API int utc_minutes_offset(const std::tm &tm = details::os::localtime());

// Return current thread id as size_t
// It exists because the std::this_thread::get_id() is much slower(especially
// under VS 2013)
SPDLOG_API size_t _thread_id() SPDLOG_NOEXCEPT;

// Return current thread id as size_t (from thread local storage)
SPDLOG_API size_t thread_id() SPDLOG_NOEXCEPT;

// This is avoid msvc issue in sleep_for that happens if the clock changes.
// See https://github.com/gabime/spdlog/issues/609
SPDLOG_API void sleep_for_millis(unsigned int milliseconds) SPDLOG_NOEXCEPT;

SPDLOG_API std::string filename_to_str(const filename_t &filename);

SPDLOG_API int pid() SPDLOG_NOEXCEPT;

// Determine if the terminal supports colors
// Source: https://github.com/agauniyal/rang/
SPDLOG_API bool is_color_terminal() SPDLOG_NOEXCEPT;

// Determine if the terminal attached
// Source: https://github.com/agauniyal/rang/
SPDLOG_API bool in_terminal(FILE *file) SPDLOG_NOEXCEPT;

#if (defined(SPDLOG_WCHAR_TO_UTF8_SUPPORT) || defined(SPDLOG_WCHAR_FILENAMES)) && defined(_WIN32)
SPDLOG_API void wstr_to_utf8buf(wstring_view_t wstr, memory_buf_t &target);

SPDLOG_API void utf8_to_wstrbuf(string_view_t str, wmemory_buf_t &target);
#endif

// Return directory name from given path or empty string
// "abc/file" => "abc"
// "abc/" => "abc"
// "abc" => ""
// "abc///" => "abc//"
SPDLOG_API filename_t dir_name(const filename_t &path);

// Create a dir from the given path.
// Return true if succeeded or if this dir already exists.
SPDLOG_API bool create_dir(const filename_t &path);

// non thread safe, cross platform getenv/getenv_s
// return empty string if field not found
SPDLOG_API std::string getenv(const char *field);

// Do fsync by FILE objectpointer.
// Return true on success.
SPDLOG_API bool fsync(FILE *fp);

// Do non-locking fwrite if possible by the os or use the regular locking fwrite
// Return true on success.
SPDLOG_API bool fwrite_bytes(const void *ptr, const size_t n_bytes, FILE *fp);

}  // namespace os
}  // namespace details
}  // namespace spdlog

#ifdef SPDLOG_HEADER_ONLY
    #include "os-inl.h"
#endif
