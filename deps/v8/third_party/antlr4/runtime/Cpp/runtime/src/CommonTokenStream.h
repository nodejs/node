/* Copyright (c) 2012-2017 The ANTLR Project. All rights reserved.
 * Use of this file is governed by the BSD 3-clause license that
 * can be found in the LICENSE.txt file in the project root.
 */

#pragma once

#include "BufferedTokenStream.h"

namespace antlr4 {

/**
 * This class extends {@link BufferedTokenStream} with functionality to filter
 * token streams to tokens on a particular channel (tokens where
 * {@link Token#getChannel} returns a particular value).
 *
 * <p>
 * This token stream provides access to all tokens by index or when calling
 * methods like {@link #getText}. The channel filtering is only used for code
 * accessing tokens via the lookahead methods {@link #LA}, {@link #LT}, and
 * {@link #LB}.</p>
 *
 * <p>
 * By default, tokens are placed on the default channel
 * ({@link Token#DEFAULT_CHANNEL}), but may be reassigned by using the
 * {@code ->channel(HIDDEN)} lexer command, or by using an embedded action to
 * call {@link Lexer#setChannel}.
 * </p>
 *
 * <p>
 * Note: lexer rules which use the {@code ->skip} lexer command or call
 * {@link Lexer#skip} do not produce tokens at all, so input text matched by
 * such a rule will not be available as part of the token stream, regardless of
 * channel.</p>
 */
class ANTLR4CPP_PUBLIC CommonTokenStream : public BufferedTokenStream {
 public:
  /**
   * Constructs a new {@link CommonTokenStream} using the specified token
   * source and the default token channel ({@link Token#DEFAULT_CHANNEL}).
   *
   * @param tokenSource The token source.
   */
  CommonTokenStream(TokenSource* tokenSource);

  /**
   * Constructs a new {@link CommonTokenStream} using the specified token
   * source and filtering tokens to the specified channel. Only tokens whose
   * {@link Token#getChannel} matches {@code channel} or have the
   * {@link Token#getType} equal to {@link Token#EOF} will be returned by the
   * token stream lookahead methods.
   *
   * @param tokenSource The token source.
   * @param channel The channel to use for filtering tokens.
   */
  CommonTokenStream(TokenSource* tokenSource, size_t channel);

  virtual Token* LT(ssize_t k) override;

  /// Count EOF just once.
  virtual int getNumberOfOnChannelTokens();

 protected:
  /**
   * Specifies the channel to use for filtering tokens.
   *
   * <p>
   * The default value is {@link Token#DEFAULT_CHANNEL}, which matches the
   * default channel assigned to tokens created by the lexer.</p>
   */
  size_t channel;

  virtual ssize_t adjustSeekIndex(size_t i) override;

  virtual Token* LB(size_t k) override;
};

}  // namespace antlr4
