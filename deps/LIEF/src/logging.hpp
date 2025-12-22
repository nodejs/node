/* Copyright 2017 - 2025 R. Thomas
 * Copyright 2017 - 2025 Quarkslab
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef LIEF_PRIVATE_LOGGING_H
#define LIEF_PRIVATE_LOGGING_H
#include <memory>
#include <sstream>

#include "LIEF/logging.hpp" // Public interface
#include "LIEF/config.h"

#include "messages.hpp"

#include <spdlog/spdlog.h>
#include <spdlog/fmt/fmt.h>
#include <spdlog/fmt/ranges.h>

#define LIEF_TRACE(...) LIEF::logging::Logger::instance().trace(__VA_ARGS__)
#define LIEF_DEBUG(...) LIEF::logging::Logger::instance().debug(__VA_ARGS__)
#define LIEF_INFO(...)  LIEF::logging::Logger::instance().info(__VA_ARGS__)
#define LIEF_WARN(...)  LIEF::logging::Logger::instance().warn(__VA_ARGS__)
#define LIEF_ERR(...)   LIEF::logging::Logger::instance().err(__VA_ARGS__)

#define CHECK(X, ...)        \
  do {                       \
    if (!(X)) {              \
      LIEF_ERR(__VA_ARGS__); \
    }                        \
  } while (false)


#define CHECK_FATAL(X, ...)  \
  do {                       \
    if ((X)) {               \
      LIEF_ERR(__VA_ARGS__); \
      std::abort();          \
    }                        \
  } while (false)

#if defined (LIEF_LOGGING_DEBUG)
#define LIEF_LOG_LOCATION()                             \
  do {                                                  \
    LIEF_DEBUG("{}:{}", __FUNCTION__, __LINE__); \
  } while (false)
#else
#define LIEF_LOG_LOCATION()
#endif


namespace LIEF {
namespace logging {

class Logger {
  public:
  static constexpr auto DEFAULT_NAME = "LIEF";
  using instances_t = std::unordered_map<std::string, Logger*>;
  Logger(const Logger&) = delete;
  Logger& operator=(const Logger&) = delete;

  static Logger& instance(const char* name);
  static Logger& instance() {
    return Logger::instance(DEFAULT_NAME);
  }

  void disable() {
    if constexpr (lief_logging_support) {
      sink_->set_level(spdlog::level::off);
    }
  }

  void enable() {
    if constexpr (lief_logging_support) {
      sink_->set_level(spdlog::level::warn);
    }
  }

  void set_level(LEVEL level);

  LEVEL get_level();

  Logger& set_log_path(const std::string& path);

  void reset();

  template <typename... Args>
  void trace(const char *fmt, const Args &... args) {
    if constexpr (lief_logging_support && lief_logging_debug) {
      sink_->trace(fmt::runtime(fmt), args...);
    }
  }

  template <typename... Args>
  void debug(const char *fmt, const Args &... args) {
    if constexpr (lief_logging_support && lief_logging_debug) {
      sink_->debug(fmt::runtime(fmt), args...);
    }
  }

  template <typename... Args>
  void info(const char *fmt, const Args &... args) {
    if constexpr (lief_logging_support) {
      sink_->info(fmt::runtime(fmt), args...);
    }
  }

  template <typename... Args>
  void err(const char *fmt, const Args &... args) {
    if constexpr (lief_logging_support) {
      sink_->error(fmt::runtime(fmt), args...);
    }
  }

  template <typename... Args>
  void warn(const char *fmt, const Args &... args) {
    if constexpr (lief_logging_support) {
      sink_->warn(fmt::runtime(fmt), args...);
    }
  }

  template <typename... Args>
  void critial(const char *fmt, const Args &... args) {
    if constexpr (lief_logging_support) {
      sink_->critical(fmt::runtime(fmt), args...);
    }
  }

  void set_logger(std::shared_ptr<spdlog::logger> logger);

  spdlog::logger& sink() {
    assert(sink_ != nullptr);
    return *sink_;
  }

  ~Logger() = default;
  Logger() = delete;
  private:
  Logger(std::shared_ptr<spdlog::logger> sink) :
    sink_(sink)
  {}
  Logger(Logger&&) noexcept = default;
  Logger& operator=(Logger&&) noexcept = default;

  std::shared_ptr<spdlog::logger> sink_;
};


inline void critial(const char *msg) {
  LIEF::logging::log(LIEF::logging::LEVEL::CRITICAL, msg);
}

template <typename... Args>
void critial(const char *fmt, const Args &... args) {
  LIEF::logging::log(LIEF::logging::LEVEL::CRITICAL,
    fmt::format(fmt::runtime(fmt), args...)
  );
}

[[noreturn]] inline void terminate() {
  std::abort();
}

[[noreturn]] inline void fatal_error(const char* msg) {
  critial(msg);
  terminate();
}

template <typename... Args>
[[noreturn]] void fatal_error(const char *fmt, const Args &... args) {
  critial(fmt, args...);
  terminate();
}

inline void needs_lief_extended() {
  if constexpr (!lief_extended) {
    Logger::instance().warn(NEEDS_EXTENDED_MSG);
  }
}

class Stream : public std::stringbuf {
  public:
  Stream(LEVEL lvl) :
    lvl_(lvl)
  {}

  int sync() override {
    switch (lvl_) {
      case LEVEL::OFF:
        break;

      case LEVEL::TRACE:
      case LEVEL::DEBUG:
        {
          LIEF_DEBUG("{}", str());
          break;
        }

      case LEVEL::INFO:
        {
          LIEF_INFO("{}", str());
          break;
        }

      case LEVEL::WARN:
        {
          LIEF_WARN("{}", str());
          break;
        }
      case LEVEL::ERR:
        {
          LIEF_ERR("{}", str());
          break;
        }

      case LEVEL::CRITICAL:
        {
          critical("{}", str());
          break;
        }
    }
    str("");
    return 0;
  }
  protected:
  LEVEL lvl_ = LEVEL::OFF;
};

}
}

#endif
