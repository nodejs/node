/* Copyright (c) 2012-2017 The ANTLR Project. All rights reserved.
 * Use of this file is governed by the BSD 3-clause license that
 * can be found in the LICENSE.txt file in the project root.
 */

#pragma once

#include "TokenStream.h"

namespace antlr4 {

/**
 * This implementation of {@link TokenStream} loads tokens from a
 * {@link TokenSource} on-demand, and places the tokens in a buffer to provide
 * access to any previous token by index.
 *
 * <p>
 * This token stream ignores the value of {@link Token#getChannel}. If your
 * parser requires the token stream filter tokens to only those on a particular
 * channel, such as {@link Token#DEFAULT_CHANNEL} or
 * {@link Token#HIDDEN_CHANNEL}, use a filtering token stream such a
 * {@link CommonTokenStream}.</p>
 */
class ANTLR4CPP_PUBLIC BufferedTokenStream : public TokenStream {
 public:
  BufferedTokenStream(TokenSource* tokenSource);
  BufferedTokenStream(const BufferedTokenStream& other) = delete;

  BufferedTokenStream& operator=(const BufferedTokenStream& other) = delete;

  virtual TokenSource* getTokenSource() const override;
  virtual size_t index() override;
  virtual ssize_t mark() override;

  virtual void release(ssize_t marker) override;
  virtual void reset();
  virtual void seek(size_t index) override;

  virtual size_t size() override;
  virtual void consume() override;

  virtual Token* get(size_t i) const override;

  /// Get all tokens from start..stop inclusively.
  virtual std::vector<Token*> get(size_t start, size_t stop);

  virtual size_t LA(ssize_t i) override;
  virtual Token* LT(ssize_t k) override;

  /// Reset this token stream by setting its token source.
  virtual void setTokenSource(TokenSource* tokenSource);
  virtual std::vector<Token*> getTokens();
  virtual std::vector<Token*> getTokens(size_t start, size_t stop);

  /// <summary>
  /// Given a start and stop index, return a List of all tokens in
  ///  the token type BitSet.  Return null if no tokens were found.  This
  ///  method looks at both on and off channel tokens.
  /// </summary>
  virtual std::vector<Token*> getTokens(size_t start, size_t stop,
                                        const std::vector<size_t>& types);
  virtual std::vector<Token*> getTokens(size_t start, size_t stop,
                                        size_t ttype);

  /// Collect all tokens on specified channel to the right of
  ///  the current token up until we see a token on DEFAULT_TOKEN_CHANNEL or
  ///  EOF. If channel is -1, find any non default channel token.
  virtual std::vector<Token*> getHiddenTokensToRight(size_t tokenIndex,
                                                     ssize_t channel);

  /// <summary>
  /// Collect all hidden tokens (any off-default channel) to the right of
  ///  the current token up until we see a token on DEFAULT_TOKEN_CHANNEL
  ///  or EOF.
  /// </summary>
  virtual std::vector<Token*> getHiddenTokensToRight(size_t tokenIndex);

  /// <summary>
  /// Collect all tokens on specified channel to the left of
  ///  the current token up until we see a token on DEFAULT_TOKEN_CHANNEL.
  ///  If channel is -1, find any non default channel token.
  /// </summary>
  virtual std::vector<Token*> getHiddenTokensToLeft(size_t tokenIndex,
                                                    ssize_t channel);

  /// <summary>
  /// Collect all hidden tokens (any off-default channel) to the left of
  ///  the current token up until we see a token on DEFAULT_TOKEN_CHANNEL.
  /// </summary>
  virtual std::vector<Token*> getHiddenTokensToLeft(size_t tokenIndex);

  virtual std::string getSourceName() const override;
  virtual std::string getText() override;
  virtual std::string getText(const misc::Interval& interval) override;
  virtual std::string getText(RuleContext* ctx) override;
  virtual std::string getText(Token* start, Token* stop) override;

  /// Get all tokens from lexer until EOF.
  virtual void fill();

 protected:
  /**
   * The {@link TokenSource} from which tokens for this stream are fetched.
   */
  TokenSource* _tokenSource;

  /**
   * A collection of all tokens fetched from the token source. The list is
   * considered a complete view of the input once {@link #fetchedEOF} is set
   * to {@code true}.
   */
  std::vector<std::unique_ptr<Token>> _tokens;

  /**
   * The index into {@link #tokens} of the current token (next token to
   * {@link #consume}). {@link #tokens}{@code [}{@link #p}{@code ]} should be
   * {@link #LT LT(1)}.
   *
   * <p>This field is set to -1 when the stream is first constructed or when
   * {@link #setTokenSource} is called, indicating that the first token has
   * not yet been fetched from the token source. For additional information,
   * see the documentation of {@link IntStream} for a description of
   * Initializing Methods.</p>
   */
  // ml: since -1 requires to make this member signed for just this single
  // aspect we use a member _needSetup instead.
  //     Use bool isInitialized() to find out if this stream has started
  //     reading.
  size_t _p;

  /**
   * Indicates whether the {@link Token#EOF} token has been fetched from
   * {@link #tokenSource} and added to {@link #tokens}. This field improves
   * performance for the following cases:
   *
   * <ul>
   * <li>{@link #consume}: The lookahead check in {@link #consume} to prevent
   * consuming the EOF symbol is optimized by checking the values of
   * {@link #fetchedEOF} and {@link #p} instead of calling {@link #LA}.</li>
   * <li>{@link #fetch}: The check to prevent adding multiple EOF symbols into
   * {@link #tokens} is trivial with this field.</li>
   * <ul>
   */
  bool _fetchedEOF;

  /// <summary>
  /// Make sure index {@code i} in tokens has a token.
  /// </summary>
  /// <returns> {@code true} if a token is located at index {@code i}, otherwise
  ///    {@code false}. </returns>
  /// <seealso cref= #get(int i) </seealso>
  virtual bool sync(size_t i);

  /// <summary>
  /// Add {@code n} elements to buffer.
  /// </summary>
  /// <returns> The actual number of elements added to the buffer. </returns>
  virtual size_t fetch(size_t n);

  virtual Token* LB(size_t k);

  /// Allowed derived classes to modify the behavior of operations which change
  /// the current stream position by adjusting the target token index of a seek
  /// operation. The default implementation simply returns {@code i}. If an
  /// exception is thrown in this method, the current stream index should not be
  /// changed.
  /// <p/>
  /// For example, <seealso cref="CommonTokenStream"/> overrides this method to
  /// ensure that the seek target is always an on-channel token.
  ///
  /// <param name="i"> The target token index. </param>
  /// <returns> The adjusted target token index. </returns>
  virtual ssize_t adjustSeekIndex(size_t i);
  void lazyInit();
  virtual void setup();

  /**
   * Given a starting index, return the index of the next token on channel.
   * Return {@code i} if {@code tokens[i]} is on channel. Return the index of
   * the EOF token if there are no tokens on channel between {@code i} and
   * EOF.
   */
  virtual ssize_t nextTokenOnChannel(size_t i, size_t channel);

  /**
   * Given a starting index, return the index of the previous token on
   * channel. Return {@code i} if {@code tokens[i]} is on channel. Return -1
   * if there are no tokens on channel between {@code i} and 0.
   *
   * <p>
   * If {@code i} specifies an index at or after the EOF token, the EOF token
   * index is returned. This is due to the fact that the EOF token is treated
   * as though it were on every channel.</p>
   */
  virtual ssize_t previousTokenOnChannel(size_t i, size_t channel);

  virtual std::vector<Token*> filterForChannel(size_t from, size_t to,
                                               ssize_t channel);

  bool isInitialized() const;

 private:
  bool _needSetup;
  void InitializeInstanceFields();
};

}  // namespace antlr4
