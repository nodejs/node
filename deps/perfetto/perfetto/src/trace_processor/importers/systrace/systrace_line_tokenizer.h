/*
 * Copyright (C) 2020 The Android Open Source Project
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

#ifndef SRC_TRACE_PROCESSOR_IMPORTERS_SYSTRACE_SYSTRACE_LINE_TOKENIZER_H_
#define SRC_TRACE_PROCESSOR_IMPORTERS_SYSTRACE_SYSTRACE_LINE_TOKENIZER_H_

#include <regex>

#include "perfetto/trace_processor/status.h"

#include "src/trace_processor/importers/systrace/systrace_line.h"

namespace perfetto {
namespace trace_processor {

class SystraceLineTokenizer {
 public:
  SystraceLineTokenizer();

  util::Status Tokenize(const std::string& line, SystraceLine*);

 private:
  const std::regex line_matcher_;
};

}  // namespace trace_processor
}  // namespace perfetto

#endif  // SRC_TRACE_PROCESSOR_IMPORTERS_SYSTRACE_SYSTRACE_LINE_TOKENIZER_H_
