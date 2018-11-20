/* Copyright (c) 2012-2017 The ANTLR Project. All rights reserved.
 * Use of this file is governed by the BSD 3-clause license that
 * can be found in the LICENSE.txt file in the project root.
 */

#pragma once

#include "antlr4-common.h"

namespace antlr4 {
namespace tree {
namespace pattern {

/// <summary>
/// A chunk is either a token tag, a rule tag, or a span of literal text within
/// a tree pattern. <p/> The method <seealso
/// cref="ParseTreePatternMatcher#split(String)"/> returns a list of chunks in
/// preparation for creating a token stream by <seealso
/// cref="ParseTreePatternMatcher#tokenize(String)"/>. From there, we get a
/// parse tree from with <seealso cref="ParseTreePatternMatcher#compile(String,
/// int)"/>. These chunks are converted to <seealso cref="RuleTagToken"/>,
/// <seealso cref="TokenTagToken"/>, or the regular tokens of the text
/// surrounding the tags.
/// </summary>
class ANTLR4CPP_PUBLIC Chunk {
 public:
  Chunk() = default;
  Chunk(Chunk const&) = default;
  virtual ~Chunk();

  Chunk& operator=(Chunk const&) = default;

  /// This method returns a text representation of the tag chunk. Labeled tags
  /// are returned in the form {@code label:tag}, and unlabeled tags are
  /// returned as just the tag name.
  virtual std::string toString() {
    std::string str;
    return str;
  }
};

}  // namespace pattern
}  // namespace tree
}  // namespace antlr4
