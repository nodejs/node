/* Copyright (c) 2012-2017 The ANTLR Project. All rights reserved.
 * Use of this file is governed by the BSD 3-clause license that
 * can be found in the LICENSE.txt file in the project root.
 */

#pragma once

#include "CommonToken.h"

namespace antlr4 {
namespace tree {
namespace pattern {

/// <summary>
/// A <seealso cref="Token"/> object representing a token of a particular type;
/// e.g.,
/// {@code <ID>}. These tokens are created for <seealso cref="TagChunk"/> chunks
/// where the tag corresponds to a lexer rule or token type.
/// </summary>
class ANTLR4CPP_PUBLIC TokenTagToken : public CommonToken {
  /// <summary>
  /// This is the backing field for <seealso cref="#getTokenName"/>.
  /// </summary>
 private:
  const std::string tokenName;
  /// <summary>
  /// This is the backing field for <seealso cref="#getLabe"/>.
  /// </summary>
  const std::string label;

  /// <summary>
  /// Constructs a new instance of <seealso cref="TokenTagToken"/> for an
  /// unlabeled tag with the specified token name and type.
  /// </summary>
  /// <param name="tokenName"> The token name. </param>
  /// <param name="type"> The token type. </param>
 public:
  TokenTagToken(const std::string& tokenName,
                int type);  // this(tokenName, type, nullptr);

  /// <summary>
  /// Constructs a new instance of <seealso cref="TokenTagToken"/> with the
  /// specified token name, type, and label.
  /// </summary>
  /// <param name="tokenName"> The token name. </param>
  /// <param name="type"> The token type. </param>
  /// <param name="label"> The label associated with the token tag, or {@code
  /// null} if the token tag is unlabeled. </param>
  TokenTagToken(const std::string& tokenName, int type,
                const std::string& label);

  /// <summary>
  /// Gets the token name. </summary>
  /// <returns> The token name. </returns>
  std::string getTokenName() const;

  /// <summary>
  /// Gets the label associated with the rule tag.
  /// </summary>
  /// <returns> The name of the label associated with the rule tag, or
  /// {@code null} if this is an unlabeled rule tag. </returns>
  std::string getLabel() const;

  /// <summary>
  /// {@inheritDoc}
  /// <p/>
  /// The implementation for <seealso cref="TokenTagToken"/> returns the token
  /// tag formatted with {@code <} and {@code >} delimiters.
  /// </summary>
  virtual std::string getText() const override;

  /// <summary>
  /// {@inheritDoc}
  /// <p/>
  /// The implementation for <seealso cref="TokenTagToken"/> returns a string of
  /// the form
  /// {@code tokenName:type}.
  /// </summary>
  virtual std::string toString() const override;
};

}  // namespace pattern
}  // namespace tree
}  // namespace antlr4
