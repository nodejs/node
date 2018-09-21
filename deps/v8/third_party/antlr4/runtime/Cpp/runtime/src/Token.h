/* Copyright (c) 2012-2017 The ANTLR Project. All rights reserved.
 * Use of this file is governed by the BSD 3-clause license that
 * can be found in the LICENSE.txt file in the project root.
 */

#pragma once

#include "IntStream.h"

namespace antlr4 {

/// A token has properties: text, type, line, character position in the line
/// (so we can ignore tabs), token channel, index, and source from which
/// we obtained this token.
class ANTLR4CPP_PUBLIC Token {
 public:
  static const size_t INVALID_TYPE = 0;

  /// During lookahead operations, this "token" signifies we hit rule end ATN
  /// state and did not follow it despite needing to.
  static const size_t EPSILON = static_cast<size_t>(-2);
  static const size_t MIN_USER_TOKEN_TYPE = 1;
  static const size_t EOF = IntStream::EOF;

  virtual ~Token();

  /// All tokens go to the parser (unless skip() is called in that rule)
  /// on a particular "channel".  The parser tunes to a particular channel
  /// so that whitespace etc... can go to the parser on a "hidden" channel.
  static const size_t DEFAULT_CHANNEL = 0;

  /// Anything on different channel than DEFAULT_CHANNEL is not parsed
  /// by parser.
  static const size_t HIDDEN_CHANNEL = 1;

  /**
   * This is the minimum constant value which can be assigned to a
   * user-defined token channel.
   *
   * <p>
   * The non-negative numbers less than {@link #MIN_USER_CHANNEL_VALUE} are
   * assigned to the predefined channels {@link #DEFAULT_CHANNEL} and
   * {@link #HIDDEN_CHANNEL}.</p>
   *
   * @see Token#getChannel()
   */
  static const size_t MIN_USER_CHANNEL_VALUE = 2;

  /// Get the text of the token.
  virtual std::string getText() const = 0;

  /// Get the token type of the token
  virtual size_t getType() const = 0;

  /// The line number on which the 1st character of this token was matched,
  /// line=1..n
  virtual size_t getLine() const = 0;

  /// The index of the first character of this token relative to the
  /// beginning of the line at which it occurs, 0..n-1
  virtual size_t getCharPositionInLine() const = 0;

  /// Return the channel this token. Each token can arrive at the parser
  /// on a different channel, but the parser only "tunes" to a single channel.
  /// The parser ignores everything not on DEFAULT_CHANNEL.
  virtual size_t getChannel() const = 0;

  /// An index from 0..n-1 of the token object in the input stream.
  /// This must be valid in order to print token streams and
  /// use TokenRewriteStream.
  ///
  /// Return INVALID_INDEX to indicate that this token was conjured up since
  /// it doesn't have a valid index.
  virtual size_t getTokenIndex() const = 0;

  /// The starting character index of the token
  /// This method is optional; return INVALID_INDEX if not implemented.
  virtual size_t getStartIndex() const = 0;

  /// The last character index of the token.
  /// This method is optional; return INVALID_INDEX if not implemented.
  virtual size_t getStopIndex() const = 0;

  /// Gets the <seealso cref="TokenSource"/> which created this token.
  virtual TokenSource* getTokenSource() const = 0;

  /// Gets the <seealso cref="CharStream"/> from which this token was derived.
  virtual CharStream* getInputStream() const = 0;

  virtual std::string toString() const = 0;
};

}  // namespace antlr4
