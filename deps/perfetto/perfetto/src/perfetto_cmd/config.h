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

#ifndef SRC_PERFETTO_CMD_CONFIG_H_
#define SRC_PERFETTO_CMD_CONFIG_H_

#include <string>
#include <vector>

#include "perfetto/tracing/core/forward_decls.h"

namespace perfetto {

// As an alternative to setting a full proto/pbtxt config users may also
// pass a handful of flags for the most common settings. Turning those
// options in a proto is the job of CreateConfigFromOptions.

struct ConfigOptions {
  std::string time = "10s";
  std::string max_file_size;
  std::string buffer_size = "32mb";
  std::vector<std::string> atrace_apps;
  std::vector<std::string> categories;
};

bool CreateConfigFromOptions(const ConfigOptions& options,
                             TraceConfig* trace_config_proto);

}  // namespace perfetto

#endif  // SRC_PERFETTO_CMD_CONFIG_H_
