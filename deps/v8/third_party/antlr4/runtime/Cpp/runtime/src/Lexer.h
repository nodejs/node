/* Copyright (c) 2012-2017 The ANTLR Project. All rights reserved.
 * Use of this file is governed by the BSD 3-clause license that
 * can be found in the LICENSE.txt file in the project root.
 */

#pragma once

#include "CharStream.h"
#include "Recognizer.h"
#include "Token.h"
#include "TokenSource.h"

namespace antlr4 {

/// A lexer is recognizer that draws input symbols from a character stream.
/// lexer grammars result in a subclass of this object. A Lexer object
/// uses simplified match() and error recovery mechanisms in the interest
/// of speed.
class ANTLR4CPP_PUBLIC Lexer : public Recognizer, public TokenSource {
 public:
  static const size_t DEFAULT_MODE = 0;
  static const size_t MORE = static_cast<size_t>(-2);
  static const size_t SKIP = static_cast<size_t>(-3);

  static const size_t DEFAULT_TOKEN_CHANNEL = Token::DEFAULT_CHANNEL;
  static const size_t HIDDEN = Token::HIDDEN_CHANNEL;
  static const size_t MIN_CHAR_VALUE = 0;
  static const size_t MAX_CHAR_VALUE = 0x10FFFF;

  CharStream*
      _input;  // Pure reference, usually from statically allocated instance.

 protected:
  /// How to create token objects.
  Ref<TokenFactory<CommonToken>> _factory;

 public:
  /// The goal of all lexer rules/methods is to create a token object.
  ///  This is an instance variable as multiple rules may collaborate to
  ///  create a single token.  nextToken will return this object after
  ///  matching lexer rule(s).  If you subclass to allow multiple token
  ///  emissions, then set this to the last token to be matched or
  ///  something nonnull so that the auto token emit mechanism will not
  ///  emit another token.

  // Life cycle of a token is this:
  // Created by emit() (via the token factory) or by action code, holding
  // ownership of it. Ownership is handed over to the token stream when calling
  // nextToken().
  std::unique_ptr<Token> token;

  /// <summary>
  /// What character index in the stream did the current token start at?
  ///  Needed, for example, to get the text for current token.  Set at
  ///  the start of nextToken.
  /// </summary>
  size_t tokenStartCharIndex;

  /// <summary>
  /// The line on which the first character of the token resides </summary>
  size_t tokenStartLine;

  /// The character position of first character within the line.
  size_t tokenStartCharPositionInLine;

  /// Once we see EOF on char stream, next token will be EOF.
  /// If you have DONE : EOF ; then you see DONE EOF.
  bool hitEOF;

  /// The channel number for the current token.
  size_t channel;

  /// The token type for the current token.
  size_t type;

  // Use the vector as a stack.
  std::vector<size_t> modeStack;
  size_t mode;

  Lexer();
  Lexer(CharStream* input);
  virtual ~Lexer() {}

  virtual void reset();

  /// Return a token from this source; i.e., match a token on the char stream.
  virtual std::unique_ptr<Token> nextToken() override;

  /// Instruct the lexer to skip creating a token for current lexer rule
  /// and look for another token.  nextToken() knows to keep looking when
  /// a lexer rule finishes with token set to SKIP_TOKEN.  Recall that
  /// if token == null at end of any token rule, it creates one for you
  /// and emits it.
  virtual void skip();
  virtual void more();
  virtual void setMode(size_t m);
  virtual void pushMode(size_t m);
  virtual size_t popMode();

  template <typename T1>
  void setTokenFactory(TokenFactory<T1>* factory) {
    this->_factory = factory;
  }

  virtual Ref<TokenFactory<CommonToken>> getTokenFactory() override;

  /// Set the char stream and reset the lexer
  virtual void setInputStream(IntStream* input) override;

  virtual std::string getSourceName() override;

  virtual CharStream* getInputStream() override;

  /// By default does not support multiple emits per nextToken invocation
  /// for efficiency reasons. Subclasses can override this method, nextToken,
  /// and getToken (to push tokens into a list and pull from that list
  /// rather than a single variable as this implementation does).
  virtual void emit(std::unique_ptr<Token> newToken);

  /// The standard method called to automatically emit a token at the
  /// outermost lexical rule.  The token object should point into the
  /// char buffer start..stop.  If there is a text override in 'text',
  /// use that to set the token's text.  Override this method to emit
  /// custom Token objects or provide a new factory.
  virtual Token* emit();

  virtual Token* emitEOF();

  virtual size_t getLine() const override;

  virtual size_t getCharPositionInLine() override;

  virtual void setLine(size_t line);

  virtual void setCharPositionInLine(size_t charPositionInLine);

  /// What is the index of the current character of lookahead?
  virtual size_t getCharIndex();

  /// Return the text matched so far for the current token or any
  /// text override.
  virtual std::string getText();

  /// Set the complete text of this token; it wipes any previous
  /// changes to the text.
  virtual void setText(const std::string& text);

  /// Override if emitting multiple tokens.
  virtual std::unique_ptr<Token> getToken();

  virtual void setToken(std::unique_ptr<Token> newToken);

  virtual void setType(size_t ttype);

  virtual size_t getType();

  virtual void setChannel(size_t newChannel);

  virtual size_t getChannel();

  virtual const std::vector<std::string>& getChannelNames() const = 0;

  virtual const std::vector<std::string>& getModeNames() const = 0;

  /// Return a list of all Token objects in input char stream.
  /// Forces load of all tokens. Does not include EOF token.
  virtual std::vector<std::unique_ptr<Token>> getAllTokens();

  virtual void recover(const LexerNoViableAltException& e);

  virtual void notifyListeners(const LexerNoViableAltException& e);

  virtual std::string getErrorDisplay(const std::string& s);

  /// Lexers can normally match any char in it's vocabulary after matching
  /// a token, so do the easy thing and just kill a character and hope
  /// it all works out.  You can instead use the rule invocation stack
  /// to do sophisticated error recovery if you are in a fragment rule.
  virtual void recover(RecognitionException* re);

  /// <summary>
  /// Gets the number of syntax errors reported during parsing. This value is
  /// incremented each time <seealso cref="#notifyErrorListeners"/> is called.
  /// </summary>
  /// <seealso cref= #notifyListeners </seealso>
  virtual size_t getNumberOfSyntaxErrors();

 protected:
  /// You can set the text for the current token to override what is in
  /// the input char buffer (via setText()).
  std::string _text;

 private:
  size_t _syntaxErrors;
  void InitializeInstanceFields();
};

}  // namespace antlr4
