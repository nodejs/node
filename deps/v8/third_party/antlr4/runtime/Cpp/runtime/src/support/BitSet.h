/* Copyright (c) 2012-2017 The ANTLR Project. All rights reserved.
 * Use of this file is governed by the BSD 3-clause license that
 * can be found in the LICENSE.txt file in the project root.
 */

#pragma once

#include "antlr4-common.h"

namespace antlrcpp {

class ANTLR4CPP_PUBLIC BitSet : public std::bitset<1024> {
 public:
  size_t nextSetBit(size_t pos) const {
    for (size_t i = pos; i < size(); i++) {
      if (test(i)) {
        return i;
      }
    }

    return INVALID_INDEX;
  }

  // Prints a list of every index for which the bitset contains a bit in true.
  friend std::wostream& operator<<(std::wostream& os, const BitSet& obj) {
    os << "{";
    size_t total = obj.count();
    for (size_t i = 0; i < obj.size(); i++) {
      if (obj.test(i)) {
        os << i;
        --total;
        if (total > 1) {
          os << ", ";
        }
      }
    }

    os << "}";
    return os;
  }

  static std::string subStringRepresentation(
      const std::vector<BitSet>::iterator& begin,
      const std::vector<BitSet>::iterator& end) {
    std::string result;
    std::vector<BitSet>::iterator vectorIterator;

    for (vectorIterator = begin; vectorIterator != end; vectorIterator++) {
      result += vectorIterator->toString();
    }
    // Grab the end
    result += end->toString();

    return result;
  }

  std::string toString() {
    std::stringstream stream;
    stream << "{";
    bool valueAdded = false;
    for (size_t i = 0; i < size(); ++i) {
      if (test(i)) {
        if (valueAdded) {
          stream << ", ";
        }
        stream << i;
        valueAdded = true;
      }
    }

    stream << "}";
    return stream.str();
  }
};
}  // namespace antlrcpp
