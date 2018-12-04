// Copyright 2011 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_PARSING_DUPLICATE_FINDER_H_
#define V8_PARSING_DUPLICATE_FINDER_H_

#include <set>

namespace v8 {
namespace internal {

class Scanner;

// DuplicateFinder : Helper class to discover duplicate symbols.
//
// Allocate a DuplicateFinder for each set of symbols you want to check
// for duplicates and then pass this instance into
// Scanner::IsDuplicateSymbol(..).
//
// This class only holds the data; all actual logic is in
// Scanner::IsDuplicateSymbol.
class DuplicateFinder {
 public:
  DuplicateFinder() = default;

 private:
  friend class Scanner;

  std::set<const void*> known_symbols_;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_PARSING_DUPLICATE_FINDER_H_
