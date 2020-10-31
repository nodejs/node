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

// This is a header-only file because we want to use this in the ART plugin,
// which should not depend on any Perfetto compilation units other than the
// client API.

#ifndef INCLUDE_PERFETTO_PROFILING_PARSE_SMAPS_H_
#define INCLUDE_PERFETTO_PROFILING_PARSE_SMAPS_H_

#include <string>

#include <errno.h>
#include <stdio.h>
#include <unistd.h>

#include <inttypes.h>

namespace perfetto {
namespace profiling {

struct SmapsEntry {
  int64_t size_kb = -1;
  int64_t private_dirty_kb = -1;
  int64_t swap_kb = -1;
  std::string pathname;
};

struct SmapsParserState {
  bool parsed_header = false;
  SmapsEntry current_entry{};
};

template <typename T>
static bool ParseSmaps(FILE* f, T callback) {
  SmapsParserState state;

  size_t line_size = 1024;
  char* line = static_cast<char*>(malloc(line_size));

  for (;;) {
    errno = 0;
    ssize_t rd = getline(&line, &line_size, f);
    if (rd == -1) {
      free(line);
      if (state.parsed_header)
        callback(state.current_entry);
      return errno == 0;
    } else {
      if (line[rd - 1] == '\n') {
        line[rd - 1] = '\0';
        rd--;
      }
      if (!ParseSmapsLine(line, static_cast<size_t>(rd), &state, callback)) {
        free(line);
        return false;
      }
    }
  }
}

static inline const char* FindNthToken(const char* line,
                                       size_t n,
                                       size_t size) {
  size_t tokens = 0;
  bool parsing_token = false;
  for (size_t i = 0; i < size; ++i) {
    if (!parsing_token && line[i] != ' ') {
      parsing_token = true;
      if (tokens++ == n)
        return line + static_cast<ssize_t>(i);
    }
    if (line[i] == ' ')
      parsing_token = false;
  }
  return nullptr;
}

template <typename T>
static bool ParseSmapsLine(char* line,
                           size_t size,
                           SmapsParserState* state,
                           T callback) {
  char* first_token_end = static_cast<char*>(memchr(line, ' ', size));
  if (first_token_end == nullptr || first_token_end == line)
    return false;  // Malformed.
  bool is_header = *(first_token_end - 1) != ':';

  if (is_header) {
    if (state->parsed_header)
      callback(state->current_entry);

    state->current_entry = {};
    const char* last_token_begin = FindNthToken(line, 5u, size);
    if (last_token_begin)
      state->current_entry.pathname.assign(last_token_begin);
    state->parsed_header = true;
    return true;
  }
  if (!state->parsed_header)
    return false;

  sscanf(line, "Size: %" PRId64 " kB", &state->current_entry.size_kb);
  sscanf(line, "Swap: %" PRId64 " kB", &state->current_entry.swap_kb);
  sscanf(line, "Private_Dirty: %" PRId64 " kB",
         &state->current_entry.private_dirty_kb);
  return true;
}

}  // namespace profiling
}  // namespace perfetto

#endif  // INCLUDE_PERFETTO_PROFILING_PARSE_SMAPS_H_
