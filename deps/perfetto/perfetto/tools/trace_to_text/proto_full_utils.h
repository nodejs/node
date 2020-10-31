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

#ifndef TOOLS_TRACE_TO_TEXT_PROTO_FULL_UTILS_H_
#define TOOLS_TRACE_TO_TEXT_PROTO_FULL_UTILS_H_

// This file provides a wrapper around protobuf libraries that can be used only
// when targeting libprotobuf-full.

#include <google/protobuf/compiler/importer.h>
#include <string>

#include "perfetto/base/logging.h"

namespace perfetto {
namespace trace_to_text {

class MultiFileErrorCollectorImpl
    : public ::google::protobuf::compiler::MultiFileErrorCollector {
 public:
  ~MultiFileErrorCollectorImpl() override;
  void AddError(const std::string& filename,
                int line,
                int column,
                const std::string& message) override;

  void AddWarning(const std::string& filename,
                  int line,
                  int column,
                  const std::string& message) override;
};

}  // namespace trace_to_text
}  // namespace perfetto

#endif  // TOOLS_TRACE_TO_TEXT_PROTO_FULL_UTILS_H_
