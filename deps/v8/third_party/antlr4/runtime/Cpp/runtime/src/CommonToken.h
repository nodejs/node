/* Copyright (c) 2012-2017 The ANTLR Project. All rights reserved.
 * Use of this file is governed by the BSD 3-clause license that
 * can be found in the LICENSE.txt file in the project root.
 */

#pragma once

#include "WritableToken.h"

namespace antlr4 {

class ANTLR4CPP_PUBLIC CommonToken : public WritableToken {
 protected:
  /**
   * An empty {@link Pair} which is used as the default value of
   * {@link #source} for tokens that do not have a source.
   */
  static const std::pair<TokenSource*, CharStream*> EMPTY_SOURCE;

  /**
   * This is the backing field for {@link #getType} and {@link #setType}.
   */
  size_t _type;

  /**
   * This is the backing field for {@link #getLine} and {@link #setLine}.
   */
  size_t _line;

  /**
   * This is the backing field for {@link #getCharPositionInLine} and
   * {@link #setCharPositionInLine}.
   */
  size_t _charPositionInLine;  // set to invalid position

  /**
   * This is the backing field for {@link #getChannel} and
   * {@link #setChannel}.
   */
  size_t _channel;

  /**
   * This is the backing field for {@link #getTokenSource} and
   * {@link #getInputStream}.
   *
   * <p>
   * These properties share a field to reduce the memory footprint of
   * {@link CommonToken}. Tokens created by a {@link CommonTokenFactory} from
   * the same source and input stream share a reference to the same
   * {@link Pair} containing these values.</p>
   */

  std::pair<TokenSource*, CharStream*> _source;  // ml: pure references, usually
                                                 // from statically allocated
                                                 // classes.

  /**
   * This is the backing field for {@link #getText} when the token text is
   * explicitly set in the constructor or via {@link #setText}.
   *
   * @see #getText()
   */
  std::string _text;

  /**
   * This is the backing field for {@link #getTokenIndex} and
   * {@link #setTokenIndex}.
   */
  size_t _index;

  /**
   * This is the backing field for {@link #getStartIndex} and
   * {@link #setStartIndex}.
   */
  size_t _start;

  /**
   * This is the backing field for {@link #getStopIndex} and
   * {@link #setStopIndex}.
   */
  size_t _stop;

 public:
  /**
   * Constructs a new {@link CommonToken} with the specified token type.
   *
   * @param type The token type.
   */
  CommonToken(size_t type);
  CommonToken(std::pair<TokenSource*, CharStream*> source, size_t type,
              size_t channel, size_t start, size_t stop);

  /**
   * Constructs a new {@link CommonToken} with the specified token type and
   * text.
   *
   * @param type The token type.
   * @param text The text of the token.
   */
  CommonToken(size_t type, const std::string& text);

  /**
   * Constructs a new {@link CommonToken} as a copy of another {@link Token}.
   *
   * <p>
   * If {@code oldToken} is also a {@link CommonToken} instance, the newly
   * constructed token will share a reference to the {@link #text} field and
   * the {@link Pair} stored in {@link #source}. Otherwise, {@link #text} will
   * be assigned the result of calling {@link #getText}, and {@link #source}
   * will be constructed from the result of {@link Token#getTokenSource} and
   * {@link Token#getInputStream}.</p>
   *
   * @param oldToken The token to copy.
   */
  CommonToken(Token* oldToken);

  virtual size_t getType() const override;

  /**
   * Explicitly set the text for this token. If {code text} is not
   * {@code null}, then {@link #getText} will return this value rather than
   * extracting the text from the input.
   *
   * @param text The explicit text of the token, or {@code null} if the text
   * should be obtained from the input along with the start and stop indexes
   * of the token.
   */
  virtual void setText(const std::string& text) override;
  virtual std::string getText() const override;

  virtual void setLine(size_t line) override;
  virtual size_t getLine() const override;

  virtual size_t getCharPositionInLine() const override;
  virtual void setCharPositionInLine(size_t charPositionInLine) override;

  virtual size_t getChannel() const override;
  virtual void setChannel(size_t channel) override;

  virtual void setType(size_t type) override;

  virtual size_t getStartIndex() const override;
  virtual void setStartIndex(size_t start);

  virtual size_t getStopIndex() const override;
  virtual void setStopIndex(size_t stop);

  virtual size_t getTokenIndex() const override;
  virtual void setTokenIndex(size_t index) override;

  virtual TokenSource* getTokenSource() const override;
  virtual CharStream* getInputStream() const override;

  virtual std::string toString() const override;

  virtual std::string toString(Recognizer* r) const;

 private:
  void InitializeInstanceFields();
};

}  // namespace antlr4
