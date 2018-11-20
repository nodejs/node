/* Copyright (c) 2012-2017 The ANTLR Project. All rights reserved.
 * Use of this file is governed by the BSD 3-clause license that
 * can be found in the LICENSE.txt file in the project root.
 */

#pragma once

#include "ProxyErrorListener.h"

namespace antlr4 {

class ANTLR4CPP_PUBLIC Recognizer {
 public:
  static const size_t EOF = static_cast<size_t>(
      -1);  // std::numeric_limits<size_t>::max(); doesn't work in VS 2013.

  Recognizer();
  Recognizer(Recognizer const&) = delete;
  virtual ~Recognizer();

  Recognizer& operator=(Recognizer const&) = delete;

  /** Used to print out token names like ID during debugging and
   *  error reporting.  The generated parsers implement a method
   *  that overrides this to point to their String[] tokenNames.
   *
   * @deprecated Use {@link #getVocabulary()} instead.
   */
  virtual std::vector<std::string> const& getTokenNames() const = 0;
  virtual std::vector<std::string> const& getRuleNames() const = 0;

  /**
   * Get the vocabulary used by the recognizer.
   *
   * @return A {@link Vocabulary} instance providing information about the
   * vocabulary used by the grammar.
   */
  virtual dfa::Vocabulary const& getVocabulary() const;

  /// <summary>
  /// Get a map from token names to token types.
  /// <p/>
  /// Used for XPath and tree pattern compilation.
  /// </summary>
  virtual std::map<std::string, size_t> getTokenTypeMap();

  /// <summary>
  /// Get a map from rule names to rule indexes.
  /// <p/>
  /// Used for XPath and tree pattern compilation.
  /// </summary>
  virtual std::map<std::string, size_t> getRuleIndexMap();

  virtual size_t getTokenType(const std::string& tokenName);

  /// <summary>
  /// If this recognizer was generated, it will have a serialized ATN
  /// representation of the grammar.
  /// <p/>
  /// For interpreters, we don't know their serialized ATN despite having
  /// created the interpreter from it.
  /// </summary>
  virtual const std::vector<uint16_t> getSerializedATN() const {
    throw "there is no serialized ATN";
  }

  /// <summary>
  /// For debugging and other purposes, might want the grammar name.
  ///  Have ANTLR generate an implementation for this method.
  /// </summary>
  virtual std::string getGrammarFileName() const = 0;

  /// Get the ATN interpreter (in fact one of it's descendants) used by the
  /// recognizer for prediction.
  /// @returns The ATN interpreter used by the recognizer for prediction.
  template <class T>
  T* getInterpreter() const {
    return dynamic_cast<T*>(_interpreter);
  }

  /**
   * Set the ATN interpreter used by the recognizer for prediction.
   *
   * @param interpreter The ATN interpreter used by the recognizer for
   * prediction.
   */
  void setInterpreter(atn::ATNSimulator* interpreter);

  /// What is the error header, normally line/character position information?
  virtual std::string getErrorHeader(RecognitionException* e);

  /** How should a token be displayed in an error message? The default
   *  is to display just the text, but during development you might
   *  want to have a lot of information spit out.  Override in that case
   *  to use t.toString() (which, for CommonToken, dumps everything about
   *  the token). This is better than forcing you to override a method in
   *  your token objects because you don't have to go modify your lexer
   *  so that it creates a new Java type.
   *
   * @deprecated This method is not called by the ANTLR 4 Runtime. Specific
   * implementations of {@link ANTLRErrorStrategy} may provide a similar
   * feature when necessary. For example, see
   * {@link DefaultErrorStrategy#getTokenErrorDisplay}.
   */
  virtual std::string getTokenErrorDisplay(Token* t);

  /// <exception cref="NullPointerException"> if {@code listener} is {@code
  /// null}. </exception>
  virtual void addErrorListener(ANTLRErrorListener* listener);

  virtual void removeErrorListener(ANTLRErrorListener* listener);

  virtual void removeErrorListeners();

  virtual ProxyErrorListener& getErrorListenerDispatch();

  // subclass needs to override these if there are sempreds or actions
  // that the ATN interp needs to execute
  virtual bool sempred(RuleContext* localctx, size_t ruleIndex,
                       size_t actionIndex);

  virtual bool precpred(RuleContext* localctx, int precedence);

  virtual void action(RuleContext* localctx, size_t ruleIndex,
                      size_t actionIndex);

  virtual size_t getState() const;

  // Get the ATN used by the recognizer for prediction.
  virtual const atn::ATN& getATN() const = 0;

  /// <summary>
  /// Indicate that the recognizer has changed internal state that is
  ///  consistent with the ATN state passed in.  This way we always know
  ///  where we are in the ATN as the parser goes along. The rule
  ///  context objects form a stack that lets us see the stack of
  ///  invoking rules. Combine this and we have complete ATN
  ///  configuration information.
  /// </summary>
  void setState(size_t atnState);

  virtual IntStream* getInputStream() = 0;

  virtual void setInputStream(IntStream* input) = 0;

  virtual Ref<TokenFactory<CommonToken>> getTokenFactory() = 0;

  template <typename T1>
  void setTokenFactory(TokenFactory<T1>* input);

 protected:
  atn::ATNSimulator*
      _interpreter;  // Set and deleted in descendants (or the profiler).

  // Mutex to manage synchronized access for multithreading.
  std::mutex _mutex;

 private:
  static std::map<const dfa::Vocabulary*, std::map<std::string, size_t>>
      _tokenTypeMapCache;
  static std::map<std::vector<std::string>, std::map<std::string, size_t>>
      _ruleIndexMapCache;

  ProxyErrorListener _proxListener;  // Manages a collection of listeners.

  size_t _stateNumber;

  void InitializeInstanceFields();
};

}  // namespace antlr4
