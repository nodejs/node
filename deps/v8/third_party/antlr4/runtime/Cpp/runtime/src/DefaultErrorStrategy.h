/* Copyright (c) 2012-2017 The ANTLR Project. All rights reserved.
 * Use of this file is governed by the BSD 3-clause license that
 * can be found in the LICENSE.txt file in the project root.
 */

#pragma once

#include "ANTLRErrorStrategy.h"
#include "misc/IntervalSet.h"

namespace antlr4 {

/**
 * This is the default implementation of {@link ANTLRErrorStrategy} used for
 * error reporting and recovery in ANTLR parsers.
 */
class ANTLR4CPP_PUBLIC DefaultErrorStrategy : public ANTLRErrorStrategy {
 public:
  DefaultErrorStrategy();
  DefaultErrorStrategy(DefaultErrorStrategy const& other) = delete;
  virtual ~DefaultErrorStrategy();

  DefaultErrorStrategy& operator=(DefaultErrorStrategy const& other) = delete;

 protected:
  /**
   * Indicates whether the error strategy is currently "recovering from an
   * error". This is used to suppress reporting multiple error messages while
   * attempting to recover from a detected syntax error.
   *
   * @see #inErrorRecoveryMode
   */
  bool errorRecoveryMode;

  /** The index into the input stream where the last error occurred.
   * 	This is used to prevent infinite loops where an error is found
   *  but no token is consumed during recovery...another error is found,
   *  ad nauseum.  This is a failsafe mechanism to guarantee that at least
   *  one token/tree node is consumed for two errors.
   */
  int lastErrorIndex;

  misc::IntervalSet lastErrorStates;

  /// <summary>
  /// {@inheritDoc}
  /// <p/>
  /// The default implementation simply calls <seealso
  /// cref="#endErrorCondition"/> to ensure that the handler is not in error
  /// recovery mode.
  /// </summary>
 public:
  virtual void reset(Parser* recognizer) override;

  /// <summary>
  /// This method is called to enter error recovery mode when a recognition
  /// exception is reported.
  /// </summary>
  /// <param name="recognizer"> the parser instance </param>
 protected:
  virtual void beginErrorCondition(Parser* recognizer);

  /// <summary>
  /// {@inheritDoc}
  /// </summary>
 public:
  virtual bool inErrorRecoveryMode(Parser* recognizer) override;

  /// <summary>
  /// This method is called to leave error recovery mode after recovering from
  /// a recognition exception.
  /// </summary>
  /// <param name="recognizer"> </param>
 protected:
  virtual void endErrorCondition(Parser* recognizer);

  /// <summary>
  /// {@inheritDoc}
  /// <p/>
  /// The default implementation simply calls <seealso
  /// cref="#endErrorCondition"/>.
  /// </summary>
 public:
  virtual void reportMatch(Parser* recognizer) override;

  /// {@inheritDoc}
  /// <p/>
  /// The default implementation returns immediately if the handler is already
  /// in error recovery mode. Otherwise, it calls <seealso
  /// cref="#beginErrorCondition"/> and dispatches the reporting task based on
  /// the runtime type of {@code e} according to the following table.
  ///
  /// <ul>
  /// <li><seealso cref="NoViableAltException"/>: Dispatches the call to
  /// <seealso cref="#reportNoViableAlternative"/></li>
  /// <li><seealso cref="InputMismatchException"/>: Dispatches the call to
  /// <seealso cref="#reportInputMismatch"/></li>
  /// <li><seealso cref="FailedPredicateException"/>: Dispatches the call to
  /// <seealso cref="#reportFailedPredicate"/></li>
  /// <li>All other types: calls <seealso cref="Parser#notifyErrorListeners"/>
  /// to report the exception</li>
  /// </ul>
  virtual void reportError(Parser* recognizer,
                           const RecognitionException& e) override;

  /// <summary>
  /// {@inheritDoc}
  /// <p/>
  /// The default implementation resynchronizes the parser by consuming tokens
  /// until we find one in the resynchronization set--loosely the set of tokens
  /// that can follow the current rule.
  /// </summary>
  virtual void recover(Parser* recognizer, std::exception_ptr e) override;

  /**
   * The default implementation of {@link ANTLRErrorStrategy#sync} makes sure
   * that the current lookahead symbol is consistent with what were expecting
   * at this point in the ATN. You can call this anytime but ANTLR only
   * generates code to check before subrules/loops and each iteration.
   *
   * <p>Implements Jim Idle's magic sync mechanism in closures and optional
   * subrules. E.g.,</p>
   *
   * <pre>
   * a : sync ( stuff sync )* ;
   * sync : {consume to what can follow sync} ;
   * </pre>
   *
   * At the start of a sub rule upon error, {@link #sync} performs single
   * token deletion, if possible. If it can't do that, it bails on the current
   * rule and uses the default error recovery, which consumes until the
   * resynchronization set of the current rule.
   *
   * <p>If the sub rule is optional ({@code (...)?}, {@code (...)*}, or block
   * with an empty alternative), then the expected set includes what follows
   * the subrule.</p>
   *
   * <p>During loop iteration, it consumes until it sees a token that can start
   * a sub rule or what follows loop. Yes, that is pretty aggressive. We opt to
   * stay in the loop as long as possible.</p>
   *
   * <p><strong>ORIGINS</strong></p>
   *
   * <p>Previous versions of ANTLR did a poor job of their recovery within
   * loops. A single mismatch token or missing token would force the parser to
   * bail out of the entire rules surrounding the loop. So, for rule</p>
   *
   * <pre>
   * classDef : 'class' ID '{' member* '}'
   * </pre>
   *
   * input with an extra token between members would force the parser to
   * consume until it found the next class definition rather than the next
   * member definition of the current class.
   *
   * <p>This functionality cost a little bit of effort because the parser has to
   * compare token set at the start of the loop and at each iteration. If for
   * some reason speed is suffering for you, you can turn off this
   * functionality by simply overriding this method as a blank { }.</p>
   */
  virtual void sync(Parser* recognizer) override;

  /// <summary>
  /// This is called by <seealso cref="#reportError"/> when the exception is a
  /// <seealso cref="NoViableAltException"/>.
  /// </summary>
  /// <seealso cref= #reportError
  /// </seealso>
  /// <param name="recognizer"> the parser instance </param>
  /// <param name="e"> the recognition exception </param>
 protected:
  virtual void reportNoViableAlternative(Parser* recognizer,
                                         const NoViableAltException& e);

  /// <summary>
  /// This is called by <seealso cref="#reportError"/> when the exception is an
  /// <seealso cref="InputMismatchException"/>.
  /// </summary>
  /// <seealso cref= #reportError
  /// </seealso>
  /// <param name="recognizer"> the parser instance </param>
  /// <param name="e"> the recognition exception </param>
  virtual void reportInputMismatch(Parser* recognizer,
                                   const InputMismatchException& e);

  /// <summary>
  /// This is called by <seealso cref="#reportError"/> when the exception is a
  /// <seealso cref="FailedPredicateException"/>.
  /// </summary>
  /// <seealso cref= #reportError
  /// </seealso>
  /// <param name="recognizer"> the parser instance </param>
  /// <param name="e"> the recognition exception </param>
  virtual void reportFailedPredicate(Parser* recognizer,
                                     const FailedPredicateException& e);

  /**
   * This method is called to report a syntax error which requires the removal
   * of a token from the input stream. At the time this method is called, the
   * erroneous symbol is current {@code LT(1)} symbol and has not yet been
   * removed from the input stream. When this method returns,
   * {@code recognizer} is in error recovery mode.
   *
   * <p>This method is called when {@link #singleTokenDeletion} identifies
   * single-token deletion as a viable recovery strategy for a mismatched
   * input error.</p>
   *
   * <p>The default implementation simply returns if the handler is already in
   * error recovery mode. Otherwise, it calls {@link #beginErrorCondition} to
   * enter error recovery mode, followed by calling
   * {@link Parser#notifyErrorListeners}.</p>
   *
   * @param recognizer the parser instance
   */
  virtual void reportUnwantedToken(Parser* recognizer);

  /**
   * This method is called to report a syntax error which requires the
   * insertion of a missing token into the input stream. At the time this
   * method is called, the missing token has not yet been inserted. When this
   * method returns, {@code recognizer} is in error recovery mode.
   *
   * <p>This method is called when {@link #singleTokenInsertion} identifies
   * single-token insertion as a viable recovery strategy for a mismatched
   * input error.</p>
   *
   * <p>The default implementation simply returns if the handler is already in
   * error recovery mode. Otherwise, it calls {@link #beginErrorCondition} to
   * enter error recovery mode, followed by calling
   * {@link Parser#notifyErrorListeners}.</p>
   *
   * @param recognizer the parser instance
   */
  virtual void reportMissingToken(Parser* recognizer);

 public:
  /**
   * {@inheritDoc}
   *
   * <p>The default implementation attempts to recover from the mismatched input
   * by using single token insertion and deletion as described below. If the
   * recovery attempt fails, this method throws an
   * {@link InputMismatchException}.</p>
   *
   * <p><strong>EXTRA TOKEN</strong> (single token deletion)</p>
   *
   * <p>{@code LA(1)} is not what we are looking for. If {@code LA(2)} has the
   * right token, however, then assume {@code LA(1)} is some extra spurious
   * token and delete it. Then consume and return the next token (which was
   * the {@code LA(2)} token) as the successful result of the match
   * operation.</p>
   *
   * <p>This recovery strategy is implemented by {@link
   * #singleTokenDeletion}.</p>
   *
   * <p><strong>MISSING TOKEN</strong> (single token insertion)</p>
   *
   * <p>If current token (at {@code LA(1)}) is consistent with what could come
   * after the expected {@code LA(1)} token, then assume the token is missing
   * and use the parser's {@link TokenFactory} to create it on the fly. The
   * "insertion" is performed by returning the created token as the successful
   * result of the match operation.</p>
   *
   * <p>This recovery strategy is implemented by {@link
   * #singleTokenInsertion}.</p>
   *
   * <p><strong>EXAMPLE</strong></p>
   *
   * <p>For example, Input {@code i=(3;} is clearly missing the {@code ')'}.
   * When the parser returns from the nested call to {@code expr}, it will have
   * call chain:</p>
   *
   * <pre>
   * stat &rarr; expr &rarr; atom
   * </pre>
   *
   * and it will be trying to match the {@code ')'} at this point in the
   * derivation:
   *
   * <pre>
   * =&gt; ID '=' '(' INT ')' ('+' atom)* ';'
   *                    ^
   * </pre>
   *
   * The attempt to match {@code ')'} will fail when it sees {@code ';'} and
   * call {@link #recoverInline}. To recover, it sees that {@code LA(1)==';'}
   * is in the set of tokens that can follow the {@code ')'} token reference
   * in rule {@code atom}. It can assume that you forgot the {@code ')'}.
   */
  virtual Token* recoverInline(Parser* recognizer) override;

  /// <summary>
  /// This method implements the single-token insertion inline error recovery
  /// strategy. It is called by <seealso cref="#recoverInline"/> if the
  /// single-token deletion strategy fails to recover from the mismatched input.
  /// If this method returns {@code true}, {@code recognizer} will be in error
  /// recovery mode. <p/> This method determines whether or not single-token
  /// insertion is viable by checking if the {@code LA(1)} input symbol could be
  /// successfully matched if it were instead the {@code LA(2)} symbol. If this
  /// method returns
  /// {@code true}, the caller is responsible for creating and inserting a
  /// token with the correct type to produce this behavior.
  /// </summary>
  /// <param name="recognizer"> the parser instance </param>
  /// <returns> {@code true} if single-token insertion is a viable recovery
  /// strategy for the current mismatched input, otherwise {@code false}
  /// </returns>
 protected:
  virtual bool singleTokenInsertion(Parser* recognizer);

  /// <summary>
  /// This method implements the single-token deletion inline error recovery
  /// strategy. It is called by <seealso cref="#recoverInline"/> to attempt to
  /// recover from mismatched input. If this method returns null, the parser and
  /// error handler state will not have changed. If this method returns
  /// non-null,
  /// {@code recognizer} will <em>not</em> be in error recovery mode since the
  /// returned token was a successful match.
  /// <p/>
  /// If the single-token deletion is successful, this method calls
  /// <seealso cref="#reportUnwantedToken"/> to report the error, followed by
  /// <seealso cref="Parser#consume"/> to actually "delete" the extraneous
  /// token. Then, before returning <seealso cref="#reportMatch"/> is called to
  /// signal a successful match.
  /// </summary>
  /// <param name="recognizer"> the parser instance </param>
  /// <returns> the successfully matched <seealso cref="Token"/> instance if
  /// single-token deletion successfully recovers from the mismatched input,
  /// otherwise
  /// {@code null} </returns>
  virtual Token* singleTokenDeletion(Parser* recognizer);

  /// <summary>
  /// Conjure up a missing token during error recovery.
  ///
  ///  The recognizer attempts to recover from single missing
  ///  symbols. But, actions might refer to that missing symbol.
  ///  For example, x=ID {f($x);}. The action clearly assumes
  ///  that there has been an identifier matched previously and that
  ///  $x points at that token. If that token is missing, but
  ///  the next token in the stream is what we want we assume that
  ///  this token is missing and we keep going. Because we
  ///  have to return some token to replace the missing token,
  ///  we have to conjure one up. This method gives the user control
  ///  over the tokens returned for missing tokens. Mostly,
  ///  you will want to create something special for identifier
  ///  tokens. For literals such as '{' and ',', the default
  ///  action in the parser or tree parser works. It simply creates
  ///  a CommonToken of the appropriate type. The text will be the token.
  ///  If you change what tokens must be created by the lexer,
  ///  override this method to create the appropriate tokens.
  /// </summary>
  virtual Token* getMissingSymbol(Parser* recognizer);

  virtual misc::IntervalSet getExpectedTokens(Parser* recognizer);

  /// <summary>
  /// How should a token be displayed in an error message? The default
  ///  is to display just the text, but during development you might
  ///  want to have a lot of information spit out.  Override in that case
  ///  to use t.toString() (which, for CommonToken, dumps everything about
  ///  the token). This is better than forcing you to override a method in
  ///  your token objects because you don't have to go modify your lexer
  ///  so that it creates a new class.
  /// </summary>
  virtual std::string getTokenErrorDisplay(Token* t);

  virtual std::string getSymbolText(Token* symbol);

  virtual size_t getSymbolType(Token* symbol);

  virtual std::string escapeWSAndQuote(const std::string& s) const;

  /*  Compute the error recovery set for the current rule.  During
   *  rule invocation, the parser pushes the set of tokens that can
   *  follow that rule reference on the stack; this amounts to
   *  computing FIRST of what follows the rule reference in the
   *  enclosing rule. See LinearApproximator.FIRST().
   *  This local follow set only includes tokens
   *  from within the rule; i.e., the FIRST computation done by
   *  ANTLR stops at the end of a rule.
   *
   *  EXAMPLE
   *
   *  When you find a "no viable alt exception", the input is not
   *  consistent with any of the alternatives for rule r.  The best
   *  thing to do is to consume tokens until you see something that
   *  can legally follow a call to r *or* any rule that called r.
   *  You don't want the exact set of viable next tokens because the
   *  input might just be missing a token--you might consume the
   *  rest of the input looking for one of the missing tokens.
   *
   *  Consider grammar:
   *
   *  a : '[' b ']'
   *    | '(' b ')'
   *    ;
   *  b : c '^' INT ;
   *  c : ID
   *    | INT
   *    ;
   *
   *  At each rule invocation, the set of tokens that could follow
   *  that rule is pushed on a stack.  Here are the various
   *  context-sensitive follow sets:
   *
   *  FOLLOW(b1_in_a) = FIRST(']') = ']'
   *  FOLLOW(b2_in_a) = FIRST(')') = ')'
   *  FOLLOW(c_in_b) = FIRST('^') = '^'
   *
   *  Upon erroneous input "[]", the call chain is
   *
   *  a -> b -> c
   *
   *  and, hence, the follow context stack is:
   *
   *  depth     follow set       start of rule execution
   *    0         <EOF>                    a (from main())
   *    1          ']'                     b
   *    2          '^'                     c
   *
   *  Notice that ')' is not included, because b would have to have
   *  been called from a different context in rule a for ')' to be
   *  included.
   *
   *  For error recovery, we cannot consider FOLLOW(c)
   *  (context-sensitive or otherwise).  We need the combined set of
   *  all context-sensitive FOLLOW sets--the set of all tokens that
   *  could follow any reference in the call chain.  We need to
   *  resync to one of those tokens.  Note that FOLLOW(c)='^' and if
   *  we resync'd to that token, we'd consume until EOF.  We need to
   *  sync to context-sensitive FOLLOWs for a, b, and c: {']','^'}.
   *  In this case, for input "[]", LA(1) is ']' and in the set, so we would
   *  not consume anything. After printing an error, rule c would
   *  return normally.  Rule b would not find the required '^' though.
   *  At this point, it gets a mismatched token error and throws an
   *  exception (since LA(1) is not in the viable following token
   *  set).  The rule exception handler tries to recover, but finds
   *  the same recovery set and doesn't consume anything.  Rule b
   *  exits normally returning to rule a.  Now it finds the ']' (and
   *  with the successful match exits errorRecovery mode).
   *
   *  So, you can see that the parser walks up the call chain looking
   *  for the token that was a member of the recovery set.
   *
   *  Errors are not generated in errorRecovery mode.
   *
   *  ANTLR's error recovery mechanism is based upon original ideas:
   *
   *  "Algorithms + Data Structures = Programs" by Niklaus Wirth
   *
   *  and
   *
   *  "A note on error recovery in recursive descent parsers":
   *  http://portal.acm.org/citation.cfm?id=947902.947905
   *
   *  Later, Josef Grosch had some good ideas:
   *
   *  "Efficient and Comfortable Error Recovery in Recursive Descent
   *  Parsers":
   *  ftp://www.cocolab.com/products/cocktail/doca4.ps/ell.ps.zip
   *
   *  Like Grosch I implement context-sensitive FOLLOW sets that are combined
   *  at run-time upon error to avoid overhead during parsing.
   */
  virtual misc::IntervalSet getErrorRecoverySet(Parser* recognizer);

  /// <summary>
  /// Consume tokens until one matches the given token set. </summary>
  virtual void consumeUntil(Parser* recognizer, const misc::IntervalSet& set);

 private:
  std::vector<std::unique_ptr<Token>>
      _errorSymbols;  // Temporarily created token.
  void InitializeInstanceFields();
};

}  // namespace antlr4
