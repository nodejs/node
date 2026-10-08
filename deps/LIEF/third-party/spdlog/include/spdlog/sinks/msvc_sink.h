// Copyright(c) 2016 Alexander Dalshov & spdlog contributors.
// Distributed under the MIT License (http://opensource.org/licenses/MIT)

#pragma once

#if defined(_WIN32)

#include <spdlog/details/null_mutex.h>
#if defined(SPDLOG_WCHAR_TO_UTF8_SUPPORT)
#include <spdlog/details/os.h>
#endif
#include <spdlog/sinks/base_sink.h>

#include <mutex>
#include <string>

// Avoid including windows.h (https://stackoverflow.com/a/30741042)
#if defined(SPDLOG_WCHAR_TO_UTF8_SUPPORT)
extern "C" __declspec(dllimport) void __stdcall OutputDebugStringW(const wchar_t *lpOutputString);
#else
extern "C" __declspec(dllimport) void __stdcall OutputDebugStringA(const char *lpOutputString);
#endif
extern "C" __declspec(dllimport) int __stdcall IsDebuggerPresent();

namespace spdlog {
namespace sinks {
/*
 * MSVC sink (logging using OutputDebugStringA)
 */
template <typename Mutex>
class msvc_sink : public base_sink<Mutex> {
public:
    msvc_sink() = default;
    msvc_sink(bool check_debugger_present)
        : check_debugger_present_{check_debugger_present} {}

protected:
    void sink_it_(const details::log_msg &msg) override {
        if (check_debugger_present_ && !IsDebuggerPresent()) {
            return;
        }
        memory_buf_t formatted;
        base_sink<Mutex>::formatter_->format(msg, formatted);
        formatted.push_back('\0');  // add a null terminator for OutputDebugString
#if defined(SPDLOG_WCHAR_TO_UTF8_SUPPORT)
        wmemory_buf_t wformatted;
        details::os::utf8_to_wstrbuf(string_view_t(formatted.data(), formatted.size()), wformatted);
        OutputDebugStringW(wformatted.data());
#else
        OutputDebugStringA(formatted.data());
#endif
    }

    void flush_() override {}

    bool check_debugger_present_ = true;
};

using msvc_sink_mt = msvc_sink<std::mutex>;
using msvc_sink_st = msvc_sink<details::null_mutex>;

using windebug_sink_mt = msvc_sink_mt;
using windebug_sink_st = msvc_sink_st;

}  // namespace sinks
}  // namespace spdlog

#endif
