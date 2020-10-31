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

#ifndef INCLUDE_PERFETTO_EXT_BASE_STRING_SPLITTER_H_
#define INCLUDE_PERFETTO_EXT_BASE_STRING_SPLITTER_H_

#include <string>

namespace perfetto {
namespace base {

// C++ version of strtok(). Splits a string without making copies or any heap
// allocations. Destructs the original string passed in input.
// Supports the special case of using \0 as a delimiter.
// The token returned in output are valid as long as the input string is valid.
class StringSplitter {
 public:
  // Can take ownership of the string if passed via std::move(), e.g.:
  // StringSplitter(std::move(str), '\n');
  StringSplitter(std::string, char delimiter);

  // Splits a C-string. The input string will be forcefully null-terminated (so
  // str[size - 1] should be == '\0' or the last char will be truncated).
  StringSplitter(char* str, size_t size, char delimiter);

  // Splits the current token from an outer StringSplitter instance. This is to
  // chain splitters as follows:
  // for (base::StringSplitter lines(x, '\n'); ss.Next();)
  //   for (base::StringSplitter words(&lines, ' '); words.Next();)
  StringSplitter(StringSplitter*, char delimiter);

  // Returns true if a token is found (in which case it will be stored in
  // cur_token()), false if no more tokens are found.
  bool Next();

  // Returns the current token iff last call to Next() returned true. In this
  // case it guarantees that the returned string is always null terminated.
  // In all other cases (before the 1st call to Next() and after Next() returns
  // false) returns nullptr.
  char* cur_token() { return cur_; }

  // Returns the length of the current token (excluding the null terminator).
  size_t cur_token_size() const { return cur_size_; }

 private:
  StringSplitter(const StringSplitter&) = delete;
  StringSplitter& operator=(const StringSplitter&) = delete;
  void Initialize(char* str, size_t size);

  std::string str_;
  char* cur_;
  size_t cur_size_;
  char* next_;
  char* end_;  // STL-style, points one past the last char.
  const char delimiter_;
};

}  // namespace base
}  // namespace perfetto

#endif  // INCLUDE_PERFETTO_EXT_BASE_STRING_SPLITTER_H_
