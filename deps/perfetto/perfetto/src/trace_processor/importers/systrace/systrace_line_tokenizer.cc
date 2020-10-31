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

#include "src/trace_processor/importers/systrace/systrace_line_tokenizer.h"

#include "perfetto/ext/base/string_utils.h"

// On windows std::isspace if overloaded in <locale>. MSBUILD via bazel
// attempts to use that version instead of the intended one defined in
// <cctype>
#include <cctype>

namespace perfetto {
namespace trace_processor {

namespace {
std::string SubstrTrim(const std::string& input) {
  std::string s = input;
  s.erase(s.begin(), std::find_if(s.begin(), s.end(),
                                  [](char ch) { return !std::isspace(ch); }));
  s.erase(std::find_if(s.rbegin(), s.rend(),
                       [](char ch) { return !std::isspace(ch); })
              .base(),
          s.end());
  return s;
}
}  // namespace

SystraceLineTokenizer::SystraceLineTokenizer()
    : line_matcher_(std::regex(R"(-(\d+)\s+\(?\s*(\d+|-+)?\)?\s?\[(\d+)\]\s*)"
                               R"([a-zA-Z0-9.]{0,4}\s+(\d+\.\d+):\s+(\S+):)")) {
}

// TODO(hjd): This should be more robust to being passed random input.
// This can happen if we mess up detecting a gzip trace for example.
util::Status SystraceLineTokenizer::Tokenize(const std::string& buffer,
                                             SystraceLine* line) {
  // An example line from buffer looks something like the following:
  // kworker/u16:1-77    (   77) [004] ....   316.196720: 0:
  // B|77|__scm_call_armv8_64|0
  //
  // However, sometimes the tgid can be missing and buffer looks like this:
  // <idle>-0     [000] ...2     0.002188: task_newtask: pid=1 ...
  //
  // Also the irq fields can be missing (we don't parse these anyway)
  // <idle>-0     [000]  0.002188: task_newtask: pid=1 ...
  //
  // The task name can contain any characters e.g -:[(/ and for this reason
  // it is much easier to use a regex (even though it is slower than parsing
  // manually)

  std::smatch matches;
  bool matched = std::regex_search(buffer, matches, line_matcher_);
  if (!matched) {
    return util::ErrStatus("Not a known systrace event format (line: %s)",
                           buffer.c_str());
  }

  std::string pid_str = matches[1].str();
  std::string cpu_str = matches[3].str();
  std::string ts_str = matches[4].str();

  line->task = SubstrTrim(matches.prefix());
  line->tgid_str = matches[2].str();
  line->event_name = matches[5].str();
  line->args_str = SubstrTrim(matches.suffix());

  base::Optional<uint32_t> maybe_pid = base::StringToUInt32(pid_str);
  if (!maybe_pid.has_value()) {
    return util::Status("Could not convert pid " + pid_str);
  }
  line->pid = maybe_pid.value();

  base::Optional<uint32_t> maybe_cpu = base::StringToUInt32(cpu_str);
  if (!maybe_cpu.has_value()) {
    return util::Status("Could not convert cpu " + cpu_str);
  }
  line->cpu = maybe_cpu.value();

  base::Optional<double> maybe_ts = base::StringToDouble(ts_str);
  if (!maybe_ts.has_value()) {
    return util::Status("Could not convert ts");
  }
  line->ts = static_cast<int64_t>(maybe_ts.value() * 1e9);

  return util::OkStatus();
}

}  // namespace trace_processor
}  // namespace perfetto
