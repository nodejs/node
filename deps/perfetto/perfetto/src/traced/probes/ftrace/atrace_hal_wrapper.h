/*
 * Copyright (C) 2019 The Android Open Source Project
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

#ifndef SRC_TRACED_PROBES_FTRACE_ATRACE_HAL_WRAPPER_H_
#define SRC_TRACED_PROBES_FTRACE_ATRACE_HAL_WRAPPER_H_

#include <memory>
#include <string>
#include <vector>

#include "perfetto/ext/base/scoped_file.h"

namespace perfetto {

class AtraceHalWrapper {
 public:
  AtraceHalWrapper();
  virtual ~AtraceHalWrapper();

  virtual std::vector<std::string> ListCategories();
  virtual bool EnableCategories(const std::vector<std::string>& categories);
  virtual bool DisableAllCategories();

 private:
  struct DynamicLibLoader;

  std::unique_ptr<DynamicLibLoader> lib_;
};

}  // namespace perfetto

#endif  // SRC_TRACED_PROBES_FTRACE_ATRACE_HAL_WRAPPER_H_
