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

#include "src/traced/probes/ftrace/ftrace_config_utils.h"

#include "perfetto/base/logging.h"

namespace perfetto {
namespace {

bool IsGoodFtracePunctuation(char c) {
  return c == '_' || c == '/' || c == '*';
}

bool IsGoodAtracePunctuation(char c) {
  return c == '_' || c == '.' || c == '*';
}

bool IsValidAtraceEventName(const std::string& str) {
  for (size_t i = 0; i < str.size(); i++) {
    if (!isalnum(str[i]) && !IsGoodAtracePunctuation(str[i]))
      return false;
  }
  return true;
}

bool IsValidFtraceEventName(const std::string& str) {
  int slash_count = 0;
  for (size_t i = 0; i < str.size(); i++) {
    if (!isalnum(str[i]) && !IsGoodFtracePunctuation(str[i]))
      return false;
    if (str[i] == '/') {
      slash_count++;
      // At most one '/' allowed and not at the beginning or end.
      if (slash_count > 1 || i == 0 || i == str.size() - 1)
        return false;
    }
    if (str[i] == '*' && i != str.size() - 1)
      return false;
  }
  return true;
}

}  // namespace

FtraceConfig CreateFtraceConfig(std::set<std::string> names) {
  FtraceConfig config;
  for (const std::string& name : names)
    *config.add_ftrace_events() = name;
  return config;
}

bool RequiresAtrace(const FtraceConfig& config) {
  return !config.atrace_categories().empty() || !config.atrace_apps().empty();
}

bool ValidConfig(const FtraceConfig& config) {
  for (const std::string& event_name : config.ftrace_events()) {
    if (!IsValidFtraceEventName(event_name)) {
      PERFETTO_ELOG("Bad event name '%s'", event_name.c_str());
      return false;
    }
  }
  for (const std::string& category : config.atrace_categories()) {
    if (!IsValidAtraceEventName(category)) {
      PERFETTO_ELOG("Bad category name '%s'", category.c_str());
      return false;
    }
  }
  for (const std::string& app : config.atrace_apps()) {
    if (!IsValidAtraceEventName(app)) {
      PERFETTO_ELOG("Bad app '%s'", app.c_str());
      return false;
    }
  }
  return true;
}

}  // namespace perfetto
