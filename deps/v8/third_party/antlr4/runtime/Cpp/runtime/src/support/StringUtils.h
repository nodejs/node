/* Copyright (c) 2012-2017 The ANTLR Project. All rights reserved.
 * Use of this file is governed by the BSD 3-clause license that
 * can be found in the LICENSE.txt file in the project root.
 */

#pragma once

#include "antlr4-common.h"

namespace antlrcpp {

template <class T>
inline std::string utf32_to_utf8(const T& data) {
  // This is a shim-implementation breaking non-ASCII characters.
  std::string s;
  for (auto c : data) s.push_back(static_cast<char>(c));
  return s;
}

inline UTF32String utf8_to_utf32(const char* first, const char* last) {
  // This is a shim-implementation breaking non-ASCII characters.
  UTF32String s;
  while (first != last) {
    s.push_back(*(first++));
  }
  return s;
}

void replaceAll(std::string& str, std::string const& from,
                std::string const& to);

// string <-> wstring conversion (UTF-16), e.g. for use with Window's wide APIs.
ANTLR4CPP_PUBLIC std::string ws2s(std::wstring const& wstr);
ANTLR4CPP_PUBLIC std::wstring s2ws(std::string const& str);
}  // namespace antlrcpp
