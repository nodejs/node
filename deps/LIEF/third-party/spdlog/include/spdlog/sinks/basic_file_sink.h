// Copyright(c) 2015-present, Gabi Melman & spdlog contributors.
// Distributed under the MIT License (http://opensource.org/licenses/MIT)

#pragma once

#include <spdlog/details/file_helper.h>
#include <spdlog/details/null_mutex.h>
#include <spdlog/details/synchronous_factory.h>
#include <spdlog/sinks/base_sink.h>

#include <mutex>
#include <string>

namespace spdlog {
namespace sinks {
/*
 * Trivial file sink with single file as target
 */
template <typename Mutex>
class basic_file_sink final : public base_sink<Mutex> {
public:
    explicit basic_file_sink(const filename_t &filename,
                             bool truncate = false,
                             const file_event_handlers &event_handlers = {});
    const filename_t &filename() const;
    void truncate();

protected:
    void sink_it_(const details::log_msg &msg) override;
    void flush_() override;

private:
    details::file_helper file_helper_;
};

using basic_file_sink_mt = basic_file_sink<std::mutex>;
using basic_file_sink_st = basic_file_sink<details::null_mutex>;

}  // namespace sinks

//
// factory functions
//
template <typename Factory = spdlog::synchronous_factory>
inline std::shared_ptr<logger> basic_logger_mt(const std::string &logger_name,
                                               const filename_t &filename,
                                               bool truncate = false,
                                               const file_event_handlers &event_handlers = {}) {
    return Factory::template create<sinks::basic_file_sink_mt>(logger_name, filename, truncate,
                                                               event_handlers);
}

template <typename Factory = spdlog::synchronous_factory>
inline std::shared_ptr<logger> basic_logger_st(const std::string &logger_name,
                                               const filename_t &filename,
                                               bool truncate = false,
                                               const file_event_handlers &event_handlers = {}) {
    return Factory::template create<sinks::basic_file_sink_st>(logger_name, filename, truncate,
                                                               event_handlers);
}

}  // namespace spdlog

#ifdef SPDLOG_HEADER_ONLY
#include "basic_file_sink-inl.h"
#endif
