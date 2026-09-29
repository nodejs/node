// Copyright(c) 2015-present, Gabi Melman & spdlog contributors.
// Distributed under the MIT License (http://opensource.org/licenses/MIT)

#pragma once

#include <spdlog/details/log_msg.h>
#include <spdlog/formatter.h>

namespace spdlog {

namespace sinks {
class SPDLOG_API sink {
public:
    virtual ~sink() = default;
    virtual void log(const details::log_msg &msg) = 0;
    virtual void flush() = 0;
    virtual void set_pattern(const std::string &pattern) = 0;
    virtual void set_formatter(std::unique_ptr<spdlog::formatter> sink_formatter) = 0;

    void set_level(level::level_enum log_level);
    level::level_enum level() const;
    bool should_log(level::level_enum msg_level) const;

protected:
    // sink log level - default is all
    level_t level_{level::trace};
};

}  // namespace sinks
}  // namespace spdlog

#ifdef SPDLOG_HEADER_ONLY
#include "sink-inl.h"
#endif
