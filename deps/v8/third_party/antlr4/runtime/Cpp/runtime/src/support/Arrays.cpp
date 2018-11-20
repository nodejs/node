/* Copyright (c) 2012-2017 The ANTLR Project. All rights reserved.
 * Use of this file is governed by the BSD 3-clause license that
 * can be found in the LICENSE.txt file in the project root.
 */

#include "Exceptions.h"
#include "tree/ParseTree.h"

#include "support/Arrays.h"

using namespace antlrcpp;

std::string Arrays::listToString(const std::vector<std::string>& list,
                                 const std::string& separator) {
  std::stringstream ss;
  bool firstEntry = true;

  ss << '[';
  for (auto& entry : list) {
    ss << entry;
    if (firstEntry) {
      ss << separator;
      firstEntry = false;
    }
  }

  ss << ']';
  return ss.str();
}

template <>
std::string Arrays::toString(
    const std::vector<antlr4::tree::ParseTree*>& source) {
  std::string result = "[";
  bool firstEntry = true;
  for (auto value : source) {
    result += value->toStringTree();
    if (firstEntry) {
      result += ", ";
      firstEntry = false;
    }
  }
  return result + "]";
}
