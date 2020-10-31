/*
 * Copyright (C) 2018 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "src/perfetto_cmd/config.h"

#include <stdlib.h>

#include "perfetto/base/logging.h"
#include "perfetto/tracing/core/data_source_config.h"
#include "perfetto/tracing/core/trace_config.h"

#include "protos/perfetto/config/ftrace/ftrace_config.gen.h"

namespace perfetto {
namespace {
using ValueUnit = std::pair<uint64_t, std::string>;
using UnitMultipler = std::pair<const char*, uint64_t>;

bool SplitValueAndUnit(const std::string& arg, ValueUnit* out) {
  char* end;
  if (!arg.size())
    return false;
  out->first = strtoull(arg.c_str(), &end, 10);
  if (end == arg.data())
    return false;
  std::string unit = arg.substr(static_cast<size_t>(end - arg.data()));
  out->second = std::move(unit);
  return true;
}

bool ConvertValue(const std::string& arg,
                  std::vector<UnitMultipler> units,
                  uint64_t* out) {
  if (arg.empty()) {
    *out = 0;
    return true;
  }

  ValueUnit value_unit{};
  if (!SplitValueAndUnit(arg, &value_unit))
    return false;

  for (const auto& unit_multiplier : units) {
    if (value_unit.second != unit_multiplier.first)
      continue;
    *out = value_unit.first * unit_multiplier.second;
    return true;
  }
  return false;
}

bool ConvertTimeToMs(const std::string& arg, uint64_t* out) {
  return ConvertValue(
      arg,
      {
          {"ms", 1}, {"s", 1000}, {"m", 1000 * 60}, {"h", 1000 * 60 * 60},
      },
      out);
}

bool ConvertSizeToKb(const std::string& arg, uint64_t* out) {
  return ConvertValue(arg,
                      {
                          {"kb", 1},
                          {"mb", 1024},
                          {"gb", 1024 * 1024},
                          {"k", 1},
                          {"m", 1024},
                          {"g", 1024 * 1024},
                      },
                      out);
}

}  // namespace

bool CreateConfigFromOptions(const ConfigOptions& options,
                             TraceConfig* config) {
  uint64_t duration_ms = 0;
  if (!ConvertTimeToMs(options.time, &duration_ms)) {
    PERFETTO_ELOG("--time argument is invalid");
    return false;
  }

  uint64_t buffer_size_kb = 0;
  if (!ConvertSizeToKb(options.buffer_size, &buffer_size_kb)) {
    PERFETTO_ELOG("--buffer argument is invalid");
    return false;
  }

  uint64_t max_file_size_kb = 0;
  if (!ConvertSizeToKb(options.max_file_size, &max_file_size_kb)) {
    PERFETTO_ELOG("--size argument is invalid");
    return false;
  }

  std::vector<std::string> ftrace_events;
  std::vector<std::string> atrace_categories;
  std::vector<std::string> atrace_apps = options.atrace_apps;

  for (const auto& category : options.categories) {
    if (category.find("/") == std::string::npos) {
      atrace_categories.push_back(category);
    } else {
      ftrace_events.push_back(category);
    }
  }

  config->set_duration_ms(static_cast<unsigned int>(duration_ms));
  config->set_max_file_size_bytes(max_file_size_kb * 1024);
  config->set_flush_period_ms(30 * 1000);
  if (max_file_size_kb)
    config->set_write_into_file(true);
  config->add_buffers()->set_size_kb(static_cast<unsigned int>(buffer_size_kb));
  auto* ds_config = config->add_data_sources()->mutable_config();
  ds_config->set_name("linux.ftrace");
  protos::gen::FtraceConfig ftrace_cfg;
  for (const auto& evt : ftrace_events)
    ftrace_cfg.add_ftrace_events(evt);
  for (const auto& cat : atrace_categories)
    ftrace_cfg.add_atrace_categories(cat);
  for (const auto& app : atrace_apps)
    ftrace_cfg.add_atrace_apps(app);
  ds_config->set_ftrace_config_raw(ftrace_cfg.SerializeAsString());

  auto* ps_config = config->add_data_sources()->mutable_config();
  ps_config->set_name("linux.process_stats");
  ps_config->set_target_buffer(0);

  return true;
}

}  // namespace perfetto
