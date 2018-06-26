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
/// Represents a placeholder tag in a tree pattern. A tag can have any of the
/// following forms.
///
/// <ul>
/// <li>{@code expr}: An unlabeled placeholder for a parser rule {@code
/// expr}.</li> <li>{@code ID}: An unlabeled placeholder for a token of type
/// {@code ID}.</li> <li>{@code e:expr}: A labeled placeholder for a parser rule
/// {@code expr}.</li> <li>{@code id:ID}: A labeled placeholder for a token of
/// type {@code ID}.</li>
/// </ul>
///
/// This class does not perform any validation on the tag or label names aside
/// from ensuring that the tag is a non-null, non-empty string.
/// </summary>
class ANTLR4CPP_PUBLIC TagChunk : public Chunk {
 public:
  /// <summary>
  /// Construct a new instance of <seealso cref="TagChunk"/> using the specified
  /// tag and no label.
  /// </summary>
  /// <param name="tag"> The tag, which should be the name of a parser rule or
  /// token type.
  /// </param>
  /// <exception cref="IllegalArgumentException"> if {@code tag} is {@code null}
  /// or empty. </exception>
  TagChunk(const std::string& tag);
  virtual ~TagChunk();

  /// <summary>
  /// Construct a new instance of <seealso cref="TagChunk"/> using the specified
  /// label and tag.
  /// </summary>
  /// <param name="label"> The label for the tag. If this is {@code null}, the
  /// <seealso cref="TagChunk"/> represents an unlabeled tag. </param>
  /// <param name="tag"> The tag, which should be the name of a parser rule or
  /// token type.
  /// </param>
  /// <exception cref="IllegalArgumentException"> if {@code tag} is {@code null}
  /// or empty. </exception>
  TagChunk(const std::string& label, const std::string& tag);

  /// <summary>
  /// Get the tag for this chunk.
  /// </summary>
  /// <returns> The tag for the chunk. </returns>
  std::string getTag();

  /// <summary>
  /// Get the label, if any, assigned to this chunk.
  /// </summary>
  /// <returns> The label assigned to this chunk, or {@code null} if no label is
  /// assigned to the chunk. </returns>
  std::string getLabel();

  /// <summary>
  /// This method returns a text representation of the tag chunk. Labeled tags
  /// are returned in the form {@code label:tag}, and unlabeled tags are
  /// returned as just the tag name.
  /// </summary>
  virtual std::string toString();

 private:
  /// This is the backing field for <seealso cref="#getTag"/>.
  const std::string _tag;
  /// <summary>
  /// This is the backing field for <seealso cref="#getLabe"/>.
  /// </summary>
  const std::string _label;
};

}  // namespace pattern
}  // namespace tree
}  // namespace antlr4
