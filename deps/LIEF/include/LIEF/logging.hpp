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
#ifndef LIEF_LOGGING_H
#define LIEF_LOGGING_H

#include "LIEF/visibility.h"

#include <string>
#include <memory>
#include <vector>
#include <cstdint>

namespace spdlog {
class logger;
}

namespace LIEF {
namespace logging {

/// **Hierarchical** logging level
///
/// From a given level set, all levels below this ! level are enabled
///
/// For example, if LEVEL::INFO is enabled then LEVEL::WARN, LEVEL::ERR are also enabled
enum class LEVEL : uint32_t {
  OFF = 0,

  TRACE,
  DEBUG,
  INFO,
  WARN,
  ERR,
  CRITICAL,
};

/// Current log level
LIEF_API LEVEL get_level();

LIEF_API const char* to_string(LEVEL e);

/// Globally disable the logging module
LIEF_API void disable();

/// Globally enable the logging module
LIEF_API void enable();

/// Change the logging level (**hierarchical**)
LIEF_API void set_level(LEVEL level);

/// Change the logger as a file-base logging and set its path
LIEF_API void set_path(const std::string& path);

/// Log a message with the LIEF's logger
LIEF_API void log(LEVEL level, const std::string& msg);

LIEF_API void log(LEVEL level, const std::string& fmt,
                  const std::vector<std::string>& args);

template <typename... Args>
void log(LEVEL level, const std::string& fmt, const Args &... args) {
  std::vector<std::string> vec_args;
  vec_args.insert(vec_args.end(), { static_cast<decltype(vec_args)::value_type>(args)...});
  return log(level, fmt, vec_args);
}

LIEF_API void set_logger(std::shared_ptr<spdlog::logger> logger);

LIEF_API void reset();

inline void enable_debug() {
  set_level(LEVEL::DEBUG);
}

inline void debug(const std::string& msg) {
  log(LEVEL::DEBUG, msg);
}

inline void debug(const std::string& fmt, const std::vector<std::string>& args) {
  log(LEVEL::DEBUG, fmt, args);
}

template <typename... Args>
void debug(const std::string& fmt, const Args &... args) {
  std::vector<std::string> vec_args;
  vec_args.insert(vec_args.end(), { static_cast<decltype(vec_args)::value_type>(args)...});
  return debug(fmt, vec_args);
}

// -----------------------------------------------------------------------------

inline void info(const std::string& msg) {
  log(LEVEL::INFO, msg);
}

inline void info(const std::string& fmt, const std::vector<std::string>& args) {
  log(LEVEL::INFO, fmt, args);
}

template <typename... Args>
void info(const std::string& fmt, const Args &... args) {
  std::vector<std::string> vec_args;
  vec_args.insert(vec_args.end(), { static_cast<decltype(vec_args)::value_type>(args)...});
  return info(fmt, vec_args);
}

// -----------------------------------------------------------------------------

inline void warn(const std::string& msg) {
  log(LEVEL::WARN, msg);
}

inline void warn(const std::string& fmt, const std::vector<std::string>& args) {
  log(LEVEL::WARN, fmt, args);
}

template <typename... Args>
void warn(const std::string& fmt, const Args &... args) {
  std::vector<std::string> vec_args;
  vec_args.insert(vec_args.end(), { static_cast<decltype(vec_args)::value_type>(args)...});
  return warn(fmt, vec_args);
}

// -----------------------------------------------------------------------------

inline void err(const std::string& msg) {
  log(LEVEL::ERR, msg);
}

inline void err(const std::string& fmt, const std::vector<std::string>& args) {
  log(LEVEL::ERR, fmt, args);
}

template <typename... Args>
void err(const std::string& fmt, const Args &... args) {
  std::vector<std::string> vec_args;
  vec_args.insert(vec_args.end(), { static_cast<decltype(vec_args)::value_type>(args)...});
  return err(fmt, vec_args);
}

// -----------------------------------------------------------------------------

inline void critical(const std::string& msg) {
  log(LEVEL::CRITICAL, msg);
}

inline void critical(const std::string& fmt, const std::vector<std::string>& args) {
  log(LEVEL::CRITICAL, fmt, args);
}

template <typename... Args>
void critical(const std::string& fmt, const Args &... args) {
  std::vector<std::string> vec_args;
  vec_args.insert(vec_args.end(), { static_cast<decltype(vec_args)::value_type>(args)...});
  return critical(fmt, vec_args);
}

// -----------------------------------------------------------------------------
namespace named {
/// Get the logging level for the logger with the given name
LIEF_API LEVEL get_level(const char* name);

/// Disable the logger with the given name
LIEF_API void disable(const char* name);

/// Enable the logger with the given name
LIEF_API void enable(const char* name);

/// Set the log level for the logger with the given name
LIEF_API void set_level(const char* name, LEVEL level);

/// Change the logger with the given as a file-base logging and set its path
LIEF_API void set_path(const char* name, const std::string& path);

/// Log a message with the logger whose name in provided in the first parameter
LIEF_API void log(const char* name, LEVEL level, const std::string& msg);

/// Set a spdlog sink for the logger with the given name
LIEF_API void set_logger(const char* name, std::shared_ptr<spdlog::logger> logger);

LIEF_API spdlog::logger& get_sink(const char* name);

/// Reset the logger with the given name
LIEF_API void reset(const char* name);

/// Enable debug logging for the logger with the given name
inline void enable_debug(const char* name) {
  set_level(name, LEVEL::DEBUG);
}

inline void debug(const char* name, const std::string& msg) {
  log(name, LEVEL::DEBUG, msg);
}

inline void info(const char* name, const std::string& msg) {
  log(name, LEVEL::INFO, msg);
}

inline void warn(const char* name, const std::string& msg) {
  log(name, LEVEL::WARN, msg);
}

inline void err(const char* name, const std::string& msg) {
  log(name, LEVEL::ERR, msg);
}

inline void critical(const char* name, const std::string& msg) {
  log(name, LEVEL::CRITICAL, msg);
}
}


class Scoped {
  public:
  Scoped(const Scoped&) = delete;
  Scoped& operator=(const Scoped&) = delete;

  Scoped(Scoped&&) = delete;
  Scoped& operator=(Scoped&&) = delete;

  explicit Scoped(LEVEL level) :
    level_(get_level())
  {
    set_level(level);
  }

  const Scoped& set_level(LEVEL lvl) const {
    logging::set_level(lvl);
    return *this;
  }

  ~Scoped() {
    set_level(level_);
  }

  private:
  LEVEL level_ = LEVEL::INFO;
};


}
}

#endif
