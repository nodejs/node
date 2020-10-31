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

#include "perfetto/ext/base/string_utils.h"

#include <string.h>

#include <algorithm>

#include "perfetto/base/logging.h"

namespace perfetto {
namespace base {

bool StartsWith(const std::string& str, const std::string& prefix) {
  return str.compare(0, prefix.length(), prefix) == 0;
}

bool EndsWith(const std::string& str, const std::string& suffix) {
  if (suffix.size() > str.size())
    return false;
  return str.compare(str.size() - suffix.size(), suffix.size(), suffix) == 0;
}

bool Contains(const std::string& haystack, const std::string& needle) {
  return haystack.find(needle) != std::string::npos;
}

size_t Find(const StringView& needle, const StringView& haystack) {
  if (needle.size() == 0)
    return 0;
  if (needle.size() > haystack.size())
    return std::string::npos;
  for (size_t i = 0; i < haystack.size() - (needle.size() - 1); ++i) {
    if (strncmp(haystack.data() + i, needle.data(), needle.size()) == 0)
      return i;
  }
  return std::string::npos;
}

bool CaseInsensitiveEqual(const std::string& first, const std::string& second) {
  return first.size() == second.size() &&
         std::equal(
             first.begin(), first.end(), second.begin(),
             [](char a, char b) { return Lowercase(a) == Lowercase(b); });
}

std::string Join(const std::vector<std::string>& parts,
                 const std::string& delim) {
  std::string acc;
  for (size_t i = 0; i < parts.size(); ++i) {
    acc += parts[i];
    if (i + 1 != parts.size()) {
      acc += delim;
    }
  }
  return acc;
}

std::vector<std::string> SplitString(const std::string& text,
                                     const std::string& delimiter) {
  PERFETTO_CHECK(!delimiter.empty());

  std::vector<std::string> output;
  size_t start = 0;
  size_t next;
  for (;;) {
    next = std::min(text.find(delimiter, start), text.size());
    if (next > start)
      output.emplace_back(&text[start], next - start);
    start = next + delimiter.size();
    if (start >= text.size())
      break;
  }
  return output;
}

std::string StripPrefix(const std::string& str, const std::string& prefix) {
  return StartsWith(str, prefix) ? str.substr(prefix.size()) : str;
}

std::string StripSuffix(const std::string& str, const std::string& suffix) {
  return EndsWith(str, suffix) ? str.substr(0, str.size() - suffix.size())
                               : str;
}

std::string ToUpper(const std::string& str) {
  // Don't use toupper(), it depends on the locale.
  std::string res(str);
  auto end = res.end();
  for (auto c = res.begin(); c != end; ++c)
    *c = Uppercase(*c);
  return res;
}

std::string ToLower(const std::string& str) {
  // Don't use tolower(), it depends on the locale.
  std::string res(str);
  auto end = res.end();
  for (auto c = res.begin(); c != end; ++c)
    *c = Lowercase(*c);
  return res;
}

std::string ToHex(const char* data, size_t size) {
  std::string hex(2 * size + 1, 'x');
  for (size_t i = 0; i < size; ++i) {
    // snprintf prints 3 characters, the two hex digits and a null byte. As we
    // write left to right, we keep overwriting the nullbytes, except for the
    // last call to snprintf.
    snprintf(&(hex[2 * i]), 3, "%02hhx", data[i]);
  }
  // Remove the trailing nullbyte produced by the last snprintf.
  hex.resize(2 * size);
  return hex;
}

std::string IntToHexString(uint32_t number) {
  size_t max_size = 11;  // Max uint32 is 0xFFFFFFFF + 1 for null byte.
  std::string buf;
  buf.resize(max_size);
  auto final_size = snprintf(&buf[0], max_size, "0x%02x", number);
  PERFETTO_DCHECK(final_size >= 0);
  buf.resize(static_cast<size_t>(final_size));  // Cuts off the final null byte.
  return buf;
}

std::string StripChars(const std::string& str,
                       const std::string& chars,
                       char replacement) {
  std::string res(str);
  const char* start = res.c_str();
  const char* remove = chars.c_str();
  for (const char* c = strpbrk(start, remove); c; c = strpbrk(c + 1, remove))
    res[static_cast<uintptr_t>(c - start)] = replacement;
  return res;
}

}  // namespace base
}  // namespace perfetto
