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

#include <stdio.h>

#include <fstream>
#include <iostream>
#include <limits>
#include <vector>

#include "perfetto/base/logging.h"
#include "perfetto/ext/base/string_utils.h"
#include "tools/trace_to_text/deobfuscate_profile.h"
#include "tools/trace_to_text/symbolize_profile.h"
#include "tools/trace_to_text/trace_to_json.h"
#include "tools/trace_to_text/trace_to_profile.h"
#include "tools/trace_to_text/trace_to_systrace.h"
#include "tools/trace_to_text/trace_to_text.h"

#if PERFETTO_BUILDFLAG(PERFETTO_VERSION_GEN)
#include "perfetto_version.gen.h"
#else
#define PERFETTO_GET_GIT_REVISION() "unknown"
#endif

#if !PERFETTO_BUILDFLAG(PERFETTO_OS_WIN)
#include <unistd.h>
#endif

namespace perfetto {
namespace trace_to_text {
namespace {

int Usage(const char* argv0) {
  fprintf(stderr,
          "Usage: %s systrace|json|ctrace|text|profile [--pid PID] "
          "[--timestamps TIMESTAMP1,TIMESTAMP2,...] "
          "[--truncate start|end] "
          "[--full-sort] "
          "[trace.pb] "
          "[trace.txt]\n"
          "\nProfile mode only:\n"
          "\t--timestamps TIMESTAMP1,TIMESTAMP2,... generate profiles "
          "only for these timestamps\n"
          "\t--pid PID generate profiles only for this process id\n",
          argv0);
  return 1;
}

uint64_t StringToUint64OrDie(const char* str) {
  char* end;
  uint64_t number = static_cast<uint64_t>(strtoll(str, &end, 10));
  if (*end != '\0') {
    PERFETTO_ELOG("Invalid %s. Expected decimal integer.", str);
    exit(1);
  }
  return number;
}

int Main(int argc, char** argv) {
  std::vector<const char*> positional_args;
  Keep truncate_keep = Keep::kAll;
  uint64_t pid = 0;
  std::vector<uint64_t> timestamps;
  bool full_sort = false;
  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--version") == 0) {
      printf("%s\n", PERFETTO_GET_GIT_REVISION());
      return 0;
    } else if (strcmp(argv[i], "-t") == 0 ||
               strcmp(argv[i], "--truncate") == 0) {
      i++;
      if (i <= argc && strcmp(argv[i], "start") == 0) {
        truncate_keep = Keep::kStart;
      } else if (i <= argc && strcmp(argv[i], "end") == 0) {
        truncate_keep = Keep::kEnd;
      } else {
        PERFETTO_ELOG(
            "--truncate must specify whether to keep the end or the "
            "start of the trace.");
        return Usage(argv[0]);
      }
    } else if (i <= argc && strcmp(argv[i], "--pid") == 0) {
      i++;
      pid = StringToUint64OrDie(argv[i]);
    } else if (i <= argc && strcmp(argv[i], "--timestamps") == 0) {
      i++;
      std::vector<std::string> ts_strings = base::SplitString(argv[i], ",");
      for (const std::string& ts : ts_strings) {
        timestamps.emplace_back(StringToUint64OrDie(ts.c_str()));
      }
    } else if (strcmp(argv[i], "--full-sort") == 0) {
      full_sort = true;
    } else {
      positional_args.push_back(argv[i]);
    }
  }

  if (positional_args.size() < 1)
    return Usage(argv[0]);

  std::istream* input_stream;
  std::ifstream file_istream;
  if (positional_args.size() > 1) {
    const char* file_path = positional_args[1];
    file_istream.open(file_path, std::ios_base::in | std::ios_base::binary);
    if (!file_istream.is_open())
      PERFETTO_FATAL("Could not open %s", file_path);
    input_stream = &file_istream;
  } else {
    if (isatty(STDIN_FILENO)) {
      PERFETTO_ELOG("Reading from stdin but it's connected to a TTY");
      PERFETTO_LOG("It is unlikely that you want to type in some binary.");
      PERFETTO_LOG("Either pass a file path to the cmdline or pipe stdin");
      return Usage(argv[0]);
    }
    input_stream = &std::cin;
  }

  std::ostream* output_stream;
  std::ofstream file_ostream;
  if (positional_args.size() > 2) {
    const char* file_path = positional_args[2];
    file_ostream.open(file_path, std::ios_base::out | std::ios_base::trunc);
    if (!file_ostream.is_open())
      PERFETTO_FATAL("Could not open %s", file_path);
    output_stream = &file_ostream;
  } else {
    output_stream = &std::cout;
  }

  std::string format(positional_args[0]);

  if (format != "profile" && (pid != 0 || !timestamps.empty())) {
    PERFETTO_ELOG(
        "--pid and --timestamps are supported only for profile format.");
    return 1;
  }

  if (format == "json")
    return TraceToJson(input_stream, output_stream, /*compress=*/false,
                       truncate_keep, full_sort);

  if (format == "systrace")
    return TraceToSystrace(input_stream, output_stream, /*ctrace=*/false,
                           truncate_keep, full_sort);

  if (format == "ctrace")
    return TraceToSystrace(input_stream, output_stream, /*ctrace=*/true,
                           truncate_keep, full_sort);

  if (truncate_keep != Keep::kAll) {
    PERFETTO_ELOG(
        "--truncate is unsupported for text|profile|symbolize format.");
    return 1;
  }

  if (full_sort) {
    PERFETTO_ELOG(
        "--full-sort is unsupported for text|profile|symbolize format.");
    return 1;
  }

  if (format == "text")
    return TraceToText(input_stream, output_stream);

  if (format == "profile")
    return TraceToProfile(input_stream, output_stream, pid, timestamps);

  if (format == "symbolize")
    return SymbolizeProfile(input_stream, output_stream);

  if (format == "deobfuscate")
    return DeobfuscateProfile(input_stream, output_stream);
  return Usage(argv[0]);
}

}  // namespace
}  // namespace trace_to_text
}  // namespace perfetto

int main(int argc, char** argv) {
  return perfetto::trace_to_text::Main(argc, argv);
}
