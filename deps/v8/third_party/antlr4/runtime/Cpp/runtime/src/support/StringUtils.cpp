/* Copyright (c) 2012-2017 The ANTLR Project. All rights reserved.
 * Use of this file is governed by the BSD 3-clause license that
 * can be found in the LICENSE.txt file in the project root.
 */

#include "support/StringUtils.h"

namespace antlrcpp {

void replaceAll(std::string& str, std::string const& from,
                std::string const& to) {
  if (from.empty()) return;

  size_t start_pos = 0;
  while ((start_pos = str.find(from, start_pos)) != std::string::npos) {
    str.replace(start_pos, from.length(), to);
    start_pos += to.length();  // In case 'to' contains 'from', like replacing
                               // 'x' with 'yx'.
  }
}

std::string ws2s(std::wstring const& wstr) {
  // This is a shim-implementation breaking non-ASCII characters.
  std::string s;
  for (wchar_t c : wstr) s.push_back(static_cast<char>(c));
  return s;
}

std::wstring s2ws(const std::string& str) {
  // This is a shim-implementation breaking non-ASCII characters.
  std::wstring s;
  for (char c : str) s.push_back(c);
  return s;
}

}  // namespace antlrcpp
