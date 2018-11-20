/* Copyright (c) 2012-2017 The ANTLR Project. All rights reserved.
 * Use of this file is governed by the BSD 3-clause license that
 * can be found in the LICENSE.txt file in the project root.
 */

#pragma once

#include "Token.h"

namespace antlr4 {

/// <summary>
/// The interface for defining strategies to deal with syntax errors encountered
/// during a parse by ANTLR-generated parsers. We distinguish between three
/// different kinds of errors:
///
/// <ul>
/// <li>The parser could not figure out which path to take in the ATN (none of
/// the available alternatives could possibly match)</li>
/// <li>The current input does not match what we were looking for</li>
/// <li>A predicate evaluated to false</li>
/// </ul>
///
/// Implementations of this interface report syntax errors by calling
/// <seealso cref="Parser#notifyErrorListeners"/>.
/// <p/>
/// TO_DO: what to do about lexers
/// </summary>
class ANTLR4CPP_PUBLIC ANTLRErrorStrategy {
 public:
  /// <summary>
  /// Reset the error handler state for the specified {@code recognizer}.
  /// </summary> <param name="recognizer"> the parser instance </param>
  virtual ~ANTLRErrorStrategy();

  virtual void reset(Parser* recognizer) = 0;

  /**
   * This method is called when an unexpected symbol is encountered during an
   * inline match operation, such as {@link Parser#match}. If the error
   * strategy successfully recovers from the match failure, this method
   * returns the {@link Token} instance which should be treated as the
   * successful result of the match.
   *
   * <p>This method handles the consumption of any tokens - the caller should
   * <b>not</b> call {@link Parser#consume} after a successful recovery.</p>
   *
   * <p>Note that the calling code will not report an error if this method
   * returns successfully. The error strategy implementation is responsible
   * for calling {@link Parser#notifyErrorListeners} as appropriate.</p>
   *
   * @param recognizer the parser instance
   * @throws RecognitionException if the error strategy was not able to
   * recover from the unexpected input symbol
   */
  virtual Token* recoverInline(Parser* recognizer) = 0;

  /// <summary>
  /// This method is called to recover from exception {@code e}. This method is
  /// called after <seealso cref="#reportError"/> by the default exception
  /// handler generated for a rule method.
  /// </summary>
  /// <seealso cref= #reportError
  /// </seealso>
  /// <param name="recognizer"> the parser instance </param>
  /// <param name="e"> the recognition exception to recover from </param>
  /// <exception cref="RecognitionException"> if the error strategy could not
  /// recover from the recognition exception </exception>
  virtual void recover(Parser* recognizer, std::exception_ptr e) = 0;

  /// <summary>
  /// This method provides the error handler with an opportunity to handle
  /// syntactic or semantic errors in the input stream before they result in a
  /// <seealso cref="RecognitionException"/>.
  /// <p/>
  /// The generated code currently contains calls to <seealso cref="#sync"/>
  /// after entering the decision state of a closure block ({@code (...)*} or
  /// {@code (...)+}).
  /// <p/>
  /// For an implementation based on Jim Idle's "magic sync" mechanism, see
  /// <seealso cref="DefaultErrorStrategy#sync"/>.
  /// </summary>
  /// <seealso cref= DefaultErrorStrategy#sync
  /// </seealso>
  /// <param name="recognizer"> the parser instance </param>
  /// <exception cref="RecognitionException"> if an error is detected by the
  /// error strategy but cannot be automatically recovered at the current state
  /// in the parsing process </exception>
  virtual void sync(Parser* recognizer) = 0;

  /// <summary>
  /// Tests whether or not {@code recognizer} is in the process of recovering
  /// from an error. In error recovery mode, <seealso cref="Parser#consume"/>
  /// adds symbols to the parse tree by calling
  /// {@link Parser#createErrorNode(ParserRuleContext, Token)} then
  /// {@link ParserRuleContext#addErrorNode(ErrorNode)} instead of
  /// {@link Parser#createTerminalNode(ParserRuleContext, Token)}.
  /// </summary>
  /// <param name="recognizer"> the parser instance </param>
  /// <returns> {@code true} if the parser is currently recovering from a parse
  /// error, otherwise {@code false} </returns>
  virtual bool inErrorRecoveryMode(Parser* recognizer) = 0;

  /// <summary>
  /// This method is called by when the parser successfully matches an input
  /// symbol.
  /// </summary>
  /// <param name="recognizer"> the parser instance </param>
  virtual void reportMatch(Parser* recognizer) = 0;

  /// <summary>
  /// Report any kind of <seealso cref="RecognitionException"/>. This method is
  /// called by the default exception handler generated for a rule method.
  /// </summary>
  /// <param name="recognizer"> the parser instance </param>
  /// <param name="e"> the recognition exception to report </param>
  virtual void reportError(Parser* recognizer,
                           const RecognitionException& e) = 0;
};

}  // namespace antlr4
