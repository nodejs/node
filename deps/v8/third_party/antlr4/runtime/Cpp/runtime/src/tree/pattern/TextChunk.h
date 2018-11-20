/* Copyright (c) 2012-2017 The ANTLR Project. All rights reserved.
 * Use of this file is governed by the BSD 3-clause license that
 * can be found in the LICENSE.txt file in the project root.
 */

#pragma once

#include "Chunk.h"

namespace antlr4 {
namespace tree {
namespace pattern {

/// <summary>
/// Represents a span of raw text (concrete syntax) between tags in a tree
/// pattern string.
/// </summary>
class ANTLR4CPP_PUBLIC TextChunk : public Chunk {
 private:
  /// <summary>
  /// This is the backing field for <seealso cref="#getText"/>.
  /// </summary>
  const std::string text;

  /// <summary>
  /// Constructs a new instance of <seealso cref="TextChunk"/> with the
  /// specified text.
  /// </summary>
  /// <param name="text"> The text of this chunk. </param>
  /// <exception cref="IllegalArgumentException"> if {@code text} is {@code
  /// null}. </exception>
 public:
  TextChunk(const std::string& text);
  virtual ~TextChunk();

  /// <summary>
  /// Gets the raw text of this chunk.
  /// </summary>
  /// <returns> The text of the chunk. </returns>
  std::string getText();

  /// <summary>
  /// {@inheritDoc}
  /// <p/>
  /// The implementation for <seealso cref="TextChunk"/> returns the result of
  /// <seealso cref="#getText()"/> in single quotes.
  /// </summary>
  virtual std::string toString();
};

}  // namespace pattern
}  // namespace tree
}  // namespace antlr4
