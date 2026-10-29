// Copyright(c) 2015-present, Gabi Melman & spdlog contributors.
// Distributed under the MIT License (http://opensource.org/licenses/MIT)

#pragma once

#ifndef SPDLOG_HEADER_ONLY
#include <spdlog/sinks/stdout_sinks.h>
#endif

#include <memory>
#include <spdlog/details/console_globals.h>
#include <spdlog/pattern_formatter.h>
#include <spdlog/details/os.h>

#ifdef _WIN32
// under windows using fwrite to non-binary stream results in \r\r\n (see issue #1675)
// so instead we use ::FileWrite
#include <spdlog/details/windows_include.h>

#ifndef _USING_V110_SDK71_  // fileapi.h doesn't exist in winxp
#include <fileapi.h>        // WriteFile (..)
#endif

#include <io.h>     // _get_osfhandle(..)
#include <stdio.h>  // _fileno(..)
#endif              // _WIN32

namespace spdlog {

namespace sinks {

template <typename ConsoleMutex>
SPDLOG_INLINE stdout_sink_base<ConsoleMutex>::stdout_sink_base(FILE *file)
    : mutex_(ConsoleMutex::mutex()),
      file_(file),
      formatter_(details::make_unique<spdlog::pattern_formatter>()) {
#ifdef _WIN32
    // get windows handle from the FILE* object

    handle_ = reinterpret_cast<HANDLE>(::_get_osfhandle(::_fileno(file_)));

    // don't throw to support cases where no console is attached,
    // and let the log method to do nothing if (handle_ == INVALID_HANDLE_VALUE).
    // throw only if non stdout/stderr target is requested (probably regular file and not console).
    if (handle_ == INVALID_HANDLE_VALUE && file != stdout && file != stderr) {
        throw_spdlog_ex("spdlog::stdout_sink_base: _get_osfhandle() failed", errno);
    }
#endif  // _WIN32
}

template <typename ConsoleMutex>
SPDLOG_INLINE void stdout_sink_base<ConsoleMutex>::log(const details::log_msg &msg) {
#ifdef _WIN32
    if (handle_ == INVALID_HANDLE_VALUE) {
        return;
    }
    std::lock_guard<mutex_t> lock(mutex_);
    memory_buf_t formatted;
    formatter_->format(msg, formatted);
    auto size = static_cast<DWORD>(formatted.size());
    DWORD bytes_written = 0;
    bool ok = ::WriteFile(handle_, formatted.data(), size, &bytes_written, nullptr) != 0;
    if (!ok) {
        throw_spdlog_ex("stdout_sink_base: WriteFile() failed. GetLastError(): " +
                        std::to_string(::GetLastError()));
    }
#else
    std::lock_guard<mutex_t> lock(mutex_);
    memory_buf_t formatted;
    formatter_->format(msg, formatted);
    details::os::fwrite_bytes(formatted.data(), formatted.size(), file_);
#endif                // _WIN32
    ::fflush(file_);  // flush every line to terminal
}

template <typename ConsoleMutex>
SPDLOG_INLINE void stdout_sink_base<ConsoleMutex>::flush() {
    std::lock_guard<mutex_t> lock(mutex_);
    fflush(file_);
}

template <typename ConsoleMutex>
SPDLOG_INLINE void stdout_sink_base<ConsoleMutex>::set_pattern(const std::string &pattern) {
    std::lock_guard<mutex_t> lock(mutex_);
    formatter_ = std::unique_ptr<spdlog::formatter>(new pattern_formatter(pattern));
}

template <typename ConsoleMutex>
SPDLOG_INLINE void stdout_sink_base<ConsoleMutex>::set_formatter(
    std::unique_ptr<spdlog::formatter> sink_formatter) {
    std::lock_guard<mutex_t> lock(mutex_);
    formatter_ = std::move(sink_formatter);
}

// stdout sink
template <typename ConsoleMutex>
SPDLOG_INLINE stdout_sink<ConsoleMutex>::stdout_sink()
    : stdout_sink_base<ConsoleMutex>(stdout) {}

// stderr sink
template <typename ConsoleMutex>
SPDLOG_INLINE stderr_sink<ConsoleMutex>::stderr_sink()
    : stdout_sink_base<ConsoleMutex>(stderr) {}

}  // namespace sinks

// factory methods
template <typename Factory>
SPDLOG_INLINE std::shared_ptr<logger> stdout_logger_mt(const std::string &logger_name) {
    return Factory::template create<sinks::stdout_sink_mt>(logger_name);
}

template <typename Factory>
SPDLOG_INLINE std::shared_ptr<logger> stdout_logger_st(const std::string &logger_name) {
    return Factory::template create<sinks::stdout_sink_st>(logger_name);
}

template <typename Factory>
SPDLOG_INLINE std::shared_ptr<logger> stderr_logger_mt(const std::string &logger_name) {
    return Factory::template create<sinks::stderr_sink_mt>(logger_name);
}

template <typename Factory>
SPDLOG_INLINE std::shared_ptr<logger> stderr_logger_st(const std::string &logger_name) {
    return Factory::template create<sinks::stderr_sink_st>(logger_name);
}
}  // namespace spdlog
