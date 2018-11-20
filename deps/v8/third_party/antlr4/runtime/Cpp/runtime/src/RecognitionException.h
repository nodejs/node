/* Copyright (c) 2012-2017 The ANTLR Project. All rights reserved.
 * Use of this file is governed by the BSD 3-clause license that
 * can be found in the LICENSE.txt file in the project root.
 */

#pragma once

#include "Exceptions.h"

namespace antlr4 {

/// The root of the ANTLR exception hierarchy. In general, ANTLR tracks just
/// 3 kinds of errors: prediction errors, failed predicate errors, and
/// mismatched input errors. In each case, the parser knows where it is
/// in the input, where it is in the ATN, the rule invocation stack,
/// and what kind of problem occurred.
class ANTLR4CPP_PUBLIC RecognitionException : public RuntimeException {
 private:
  /// The Recognizer where this exception originated.
  Recognizer* _recognizer;
  IntStream* _input;
  ParserRuleContext* _ctx;

  /// The current Token when an error occurred. Since not all streams
  /// support accessing symbols by index, we have to track the Token
  /// instance itself.
  Token* _offendingToken;

  size_t _offendingState;

 public:
  RecognitionException(Recognizer* recognizer, IntStream* input,
                       ParserRuleContext* ctx, Token* offendingToken = nullptr);
  RecognitionException(const std::string& message, Recognizer* recognizer,
                       IntStream* input, ParserRuleContext* ctx,
                       Token* offendingToken = nullptr);
  RecognitionException(RecognitionException const&) = default;
  ~RecognitionException();
  RecognitionException& operator=(RecognitionException const&) = default;

  /// Get the ATN state number the parser was in at the time the error
  /// occurred. For NoViableAltException and
  /// LexerNoViableAltException exceptions, this is the
  /// DecisionState number. For others, it is the state whose outgoing
  /// edge we couldn't match.
  ///
  /// If the state number is not known, this method returns -1.
  virtual size_t getOffendingState() const;

 protected:
  void setOffendingState(size_t offendingState);

  /// Gets the set of input symbols which could potentially follow the
  /// previously matched symbol at the time this exception was thrown.
  ///
  /// If the set of expected tokens is not known and could not be computed,
  /// this method returns an empty set.
  ///
  /// @returns The set of token types that could potentially follow the current
  /// state in the ATN, or an empty set if the information is not available.
 public:
  virtual misc::IntervalSet getExpectedTokens() const;

  /// <summary>
  /// Gets the <seealso cref="RuleContext"/> at the time this exception was
  /// thrown. <p/> If the context is not available, this method returns {@code
  /// null}.
  /// </summary>
  /// <returns> The <seealso cref="RuleContext"/> at the time this exception was
  /// thrown. If the context is not available, this method returns {@code null}.
  /// </returns>
  virtual RuleContext* getCtx() const;

  /// <summary>
  /// Gets the input stream which is the symbol source for the recognizer where
  /// this exception was thrown.
  /// <p/>
  /// If the input stream is not available, this method returns {@code null}.
  /// </summary>
  /// <returns> The input stream which is the symbol source for the recognizer
  /// where this exception was thrown, or {@code null} if the stream is not
  /// available. </returns>
  virtual IntStream* getInputStream() const;

  virtual Token* getOffendingToken() const;

  /// <summary>
  /// Gets the <seealso cref="Recognizer"/> where this exception occurred.
  /// <p/>
  /// If the recognizer is not available, this method returns {@code null}.
  /// </summary>
  /// <returns> The recognizer where this exception occurred, or {@code null} if
  /// the recognizer is not available. </returns>
  virtual Recognizer* getRecognizer() const;

 private:
  void InitializeInstanceFields();
};

}  // namespace antlr4
