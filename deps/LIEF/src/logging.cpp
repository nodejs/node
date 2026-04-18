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

#include "LIEF/config.h"
#include "LIEF/logging.hpp"
#include "LIEF/platforms.hpp"
#include "logging.hpp"

#include "spdlog/spdlog.h"

#if !defined(SPDLOG_FMT_EXTERNAL)
#include "spdlog/fmt/bundled/args.h"
#else
#include "fmt/args.h"
#endif

#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/sinks/basic_file_sink.h"
#include "spdlog/sinks/android_sink.h"

namespace LIEF {
namespace logging {


std::shared_ptr<spdlog::logger>
  create_basic_logger_mt(const std::string& name, const std::string& path, bool truncate = false)
{
  spdlog::filename_t fname(path.begin(), path.end());
  return spdlog::basic_logger_mt(name, fname, truncate);
}

static std::shared_ptr<spdlog::logger> default_logger(
  [[maybe_unused]] const std::string& name = "LIEF",
  [[maybe_unused]] const std::string& logcat_tag = "",
  [[maybe_unused]] const std::string& filepath = "/tmp/lief.log",
  [[maybe_unused]] bool truncate = true
)
{
  auto& registry = spdlog::details::registry::instance();
  registry.drop(name);

  std::shared_ptr<spdlog::logger> sink;
  if constexpr (current_platform() == PLATFORMS::PLAT_ANDROID) {
#if defined(__ANDROID__)
    sink = spdlog::android_logger_mt(name, logcat_tag);
#endif
  }
  else if (current_platform() == PLATFORMS::PLAT_IOS) {
    sink = create_basic_logger_mt(name, filepath, truncate);
  }
  else {
    sink = spdlog::stderr_color_mt(name);
  }

  sink->set_level(spdlog::level::warn);
  sink->set_pattern("%v");
  sink->flush_on(spdlog::level::warn);
  return sink;
}

LEVEL Logger::get_level() {
  spdlog::level::level_enum lvl = sink_->level();
  switch (lvl) {
    default:
    case spdlog::level::level_enum::off:
      return LEVEL::OFF;
    case spdlog::level::level_enum::trace:
      return LEVEL::TRACE;
    case spdlog::level::level_enum::debug:
      return LEVEL::DEBUG;
    case spdlog::level::level_enum::info:
      return LEVEL::INFO;
    case spdlog::level::level_enum::warn:
      return LEVEL::WARN;
    case spdlog::level::level_enum::err:
      return LEVEL::ERR;
    case spdlog::level::level_enum::critical:
      return LEVEL::CRITICAL;
  }
  return LEVEL::TRACE;
}


Logger& Logger::instance(const char* name) {
  static Logger::instances_t instances;
  static std::mutex mu;
  std::lock_guard LK(mu);

  if (auto it = instances.find(name); it != instances.end()) {
    return *it->second;
  }

  if (instances.empty()) {
    std::atexit([] {
      std::lock_guard LK(mu);
      for (const auto& [name, instance] : instances) {
        delete instance;
      }
      instances.clear();
    });
  }

  auto* impl = new Logger(default_logger(/*name=*/name));
  instances.insert({name, impl});
  return *impl;
}

void Logger::reset() {
  set_logger(default_logger());
}

Logger& Logger::set_log_path(const std::string& path) {
  auto& registry = spdlog::details::registry::instance();
  registry.drop(DEFAULT_NAME);
  auto logger = create_basic_logger_mt(DEFAULT_NAME, path, /*truncate=*/true);
  set_logger(std::move(logger));
  return *this;
}

void Logger::set_logger(std::shared_ptr<spdlog::logger> logger) {
  sink_ = logger;
  sink_->set_pattern("%v");
  sink_->set_level(spdlog::level::warn);
  sink_->flush_on(spdlog::level::warn);
}

const char* to_string(LEVEL e) {
  switch (e) {
    case LEVEL::OFF: return "OFF";
    case LEVEL::TRACE: return "TRACE";
    case LEVEL::DEBUG: return "DEBUG";
    case LEVEL::INFO: return "INFO";
    case LEVEL::ERR: return "ERROR";
    case LEVEL::WARN: return "WARN";
    case LEVEL::CRITICAL: return "CRITICAL";
    default: return "UNDEFINED";
  }
  return "UNDEFINED";
}

void Logger::set_level(LEVEL level) {
  if constexpr (!lief_logging_support) {
    return;
  }
  switch (level) {
    case LEVEL::OFF:
      {
        sink_->set_level(spdlog::level::off);
        sink_->flush_on(spdlog::level::off);
        break;
      }

    case LEVEL::TRACE:
      {
        sink_->set_level(spdlog::level::trace);
        sink_->flush_on(spdlog::level::trace);
        break;
      }

    case LEVEL::DEBUG:
      {
        sink_->set_level(spdlog::level::debug);
        sink_->flush_on(spdlog::level::debug);
        break;
      }

    case LEVEL::INFO:
      {
        sink_->set_level(spdlog::level::info);
        sink_->flush_on(spdlog::level::info);
        break;
      }

    default:
    case LEVEL::WARN:
      {
        sink_->set_level(spdlog::level::warn);
        sink_->flush_on(spdlog::level::warn);
        break;
      }

    case LEVEL::ERR:
      {
        sink_->set_level(spdlog::level::err);
        sink_->flush_on(spdlog::level::err);
        break;
      }

    case LEVEL::CRITICAL:
      {
        sink_->set_level(spdlog::level::critical);
        sink_->flush_on(spdlog::level::critical);
        break;
      }
  }
}

// Public interface

void disable() {
  Logger::instance().disable();
}

void enable() {
  Logger::instance().enable();
}

void set_level(LEVEL level) {
  Logger::instance().set_level(level);
}

void set_path(const std::string& path) {
  Logger::instance().set_log_path(path);
}

void set_logger(std::shared_ptr<spdlog::logger> logger) {
  Logger::instance().set_logger(std::move(logger));
}

void reset() {
  Logger::instance().reset();
}

LEVEL get_level() {
  return Logger::instance().get_level();
}

void log(LEVEL level, const std::string& msg) {
  switch (level) {
    case LEVEL::OFF:
      break;
    case LEVEL::TRACE:
    case LEVEL::DEBUG:
      {
        LIEF_DEBUG("{}", msg);
        break;
      }
    case LEVEL::INFO:
      {
        LIEF_INFO("{}", msg);
        break;
      }
    case LEVEL::WARN:
      {
        LIEF_WARN("{}", msg);
        break;
      }
    case LEVEL::CRITICAL:
    case LEVEL::ERR:
      {
        LIEF_ERR("{}", msg);
        break;
      }
  }
}

void log(LEVEL level, const std::string& fmt,
         const std::vector<std::string>& args)
{
  fmt::dynamic_format_arg_store<fmt::format_context> store;
  for (const std::string& arg : args) {
    store.push_back(arg);
  }
  std::string result = fmt::vformat(fmt, store);
  log(level, result);
}

namespace named {
LEVEL get_level(const char* name) {
  return Logger::instance(name).get_level();
}

void disable(const char* name) {
  Logger::instance(name).disable();
}

void enable(const char* name) {
  Logger::instance(name).enable();
}

void set_level(const char* name, LEVEL level) {
  Logger::instance(name).set_level(level);
}

void set_path(const char* name, const std::string& path) {
  Logger::instance(name).set_log_path(path);
}

void log(const char* name, LEVEL level, const std::string& msg) {
  switch (level) {
    case LEVEL::OFF:
      return;
    case LEVEL::TRACE:
    case LEVEL::DEBUG:
      return Logger::instance(name).debug("{}", msg);
    case LEVEL::INFO:
      return Logger::instance(name).info("{}", msg);
    case LEVEL::WARN:
      return Logger::instance(name).warn("{}", msg);
    case LEVEL::ERR:
      return Logger::instance(name).err("{}", msg);
    case LEVEL::CRITICAL:
      return Logger::instance(name).critial("{}", msg);
  }
}

void set_logger(const char* name, std::shared_ptr<spdlog::logger> logger) {
  Logger::instance(name).set_logger(std::move(logger));
}

void reset(const char* name) {
  Logger::instance(name).reset();
}

spdlog::logger& get_sink(const char* name) {
  return Logger::instance(name).sink();
}
}

}
}


