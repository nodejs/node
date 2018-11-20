/* Copyright (c) 2012-2017 The ANTLR Project. All rights reserved.
 * Use of this file is governed by the BSD 3-clause license that
 * can be found in the LICENSE.txt file in the project root.
 */

#pragma once

#include "PredictionMode.h"
#include "SemanticContext.h"
#include "atn/ATNConfig.h"
#include "atn/ATNSimulator.h"
#include "atn/PredictionContext.h"
#include "dfa/DFAState.h"

namespace antlr4 {
namespace atn {

/**
 * The embodiment of the adaptive LL(*), ALL(*), parsing strategy.
 *
 * <p>
 * The basic complexity of the adaptive strategy makes it harder to understand.
 * We begin with ATN simulation to build paths in a DFA. Subsequent prediction
 * requests go through the DFA first. If they reach a state without an edge for
 * the current symbol, the algorithm fails over to the ATN simulation to
 * complete the DFA path for the current input (until it finds a conflict state
 * or uniquely predicting state).</p>
 *
 * <p>
 * All of that is done without using the outer context because we want to create
 * a DFA that is not dependent upon the rule invocation stack when we do a
 * prediction. One DFA works in all contexts. We avoid using context not
 * necessarily because it's slower, although it can be, but because of the DFA
 * caching problem. The closure routine only considers the rule invocation stack
 * created during prediction beginning in the decision rule. For example, if
 * prediction occurs without invoking another rule's ATN, there are no context
 * stacks in the configurations. When lack of context leads to a conflict, we
 * don't know if it's an ambiguity or a weakness in the strong LL(*) parsing
 * strategy (versus full LL(*)).</p>
 *
 * <p>
 * When SLL yields a configuration set with conflict, we rewind the input and
 * retry the ATN simulation, this time using full outer context without adding
 * to the DFA. Configuration context stacks will be the full invocation stacks
 * from the start rule. If we get a conflict using full context, then we can
 * definitively say we have a true ambiguity for that input sequence. If we
 * don't get a conflict, it implies that the decision is sensitive to the outer
 * context. (It is not context-sensitive in the sense of context-sensitive
 * grammars.)</p>
 *
 * <p>
 * The next time we reach this DFA state with an SLL conflict, through DFA
 * simulation, we will again retry the ATN simulation using full context mode.
 * This is slow because we can't save the results and have to "interpret" the
 * ATN each time we get that input.</p>
 *
 * <p>
 * <strong>CACHING FULL CONTEXT PREDICTIONS</strong></p>
 *
 * <p>
 * We could cache results from full context to predicted alternative easily and
 * that saves a lot of time but doesn't work in presence of predicates. The set
 * of visible predicates from the ATN start state changes depending on the
 * context, because closure can fall off the end of a rule. I tried to cache
 * tuples (stack context, semantic context, predicted alt) but it was slower
 * than interpreting and much more complicated. Also required a huge amount of
 * memory. The goal is not to create the world's fastest parser anyway. I'd like
 * to keep this algorithm simple. By launching multiple threads, we can improve
 * the speed of parsing across a large number of files.</p>
 *
 * <p>
 * There is no strict ordering between the amount of input used by SLL vs LL,
 * which makes it really hard to build a cache for full context. Let's say that
 * we have input A B C that leads to an SLL conflict with full context X. That
 * implies that using X we might only use A B but we could also use A B C D to
 * resolve conflict. Input A B C D could predict alternative 1 in one position
 * in the input and A B C E could predict alternative 2 in another position in
 * input. The conflicting SLL configurations could still be non-unique in the
 * full context prediction, which would lead us to requiring more input than the
 * original A B C.	To make a	prediction cache work, we have to track
 * the exact input	used during the previous prediction. That amounts to a
 * cache that maps X to a specific DFA for that context.</p>
 *
 * <p>
 * Something should be done for left-recursive expression predictions. They are
 * likely LL(1) + pred eval. Easier to do the whole SLL unless error and retry
 * with full LL thing Sam does.</p>
 *
 * <p>
 * <strong>AVOIDING FULL CONTEXT PREDICTION</strong></p>
 *
 * <p>
 * We avoid doing full context retry when the outer context is empty, we did not
 * dip into the outer context by falling off the end of the decision state rule,
 * or when we force SLL mode.</p>
 *
 * <p>
 * As an example of the not dip into outer context case, consider as super
 * constructor calls versus function calls. One grammar might look like
 * this:</p>
 *
 * <pre>
 * ctorBody
 *   : '{' superCall? stat* '}'
 *   ;
 * </pre>
 *
 * <p>
 * Or, you might see something like</p>
 *
 * <pre>
 * stat
 *   : superCall ';'
 *   | expression ';'
 *   | ...
 *   ;
 * </pre>
 *
 * <p>
 * In both cases I believe that no closure operations will dip into the outer
 * context. In the first case ctorBody in the worst case will stop at the '}'.
 * In the 2nd case it should stop at the ';'. Both cases should stay within the
 * entry rule and not dip into the outer context.</p>
 *
 * <p>
 * <strong>PREDICATES</strong></p>
 *
 * <p>
 * Predicates are always evaluated if present in either SLL or LL both. SLL and
 * LL simulation deals with predicates differently. SLL collects predicates as
 * it performs closure operations like ANTLR v3 did. It delays predicate
 * evaluation until it reaches and accept state. This allows us to cache the SLL
 * ATN simulation whereas, if we had evaluated predicates on-the-fly during
 * closure, the DFA state configuration sets would be different and we couldn't
 * build up a suitable DFA.</p>
 *
 * <p>
 * When building a DFA accept state during ATN simulation, we evaluate any
 * predicates and return the sole semantically valid alternative. If there is
 * more than 1 alternative, we report an ambiguity. If there are 0 alternatives,
 * we throw an exception. Alternatives without predicates act like they have
 * true predicates. The simple way to think about it is to strip away all
 * alternatives with false predicates and choose the minimum alternative that
 * remains.</p>
 *
 * <p>
 * When we start in the DFA and reach an accept state that's predicated, we test
 * those and return the minimum semantically viable alternative. If no
 * alternatives are viable, we throw an exception.</p>
 *
 * <p>
 * During full LL ATN simulation, closure always evaluates predicates and
 * on-the-fly. This is crucial to reducing the configuration set size during
 * closure. It hits a landmine when parsing with the Java grammar, for example,
 * without this on-the-fly evaluation.</p>
 *
 * <p>
 * <strong>SHARING DFA</strong></p>
 *
 * <p>
 * All instances of the same parser share the same decision DFAs through a
 * static field. Each instance gets its own ATN simulator but they share the
 * same {@link #decisionToDFA} field. They also share a
 * {@link PredictionContextCache} object that makes sure that all
 * {@link PredictionContext} objects are shared among the DFA states. This makes
 * a big size difference.</p>
 *
 * <p>
 * <strong>THREAD SAFETY</strong></p>
 *
 * <p>
 * The {@link ParserATNSimulator} locks on the {@link #decisionToDFA} field when
 * it adds a new DFA object to that array. {@link #addDFAEdge}
 * locks on the DFA for the current decision when setting the
 * {@link DFAState#edges} field. {@link #addDFAState} locks on
 * the DFA for the current decision when looking up a DFA state to see if it
 * already exists. We must make sure that all requests to add DFA states that
 * are equivalent result in the same shared DFA object. This is because lots of
 * threads will be trying to update the DFA at once. The
 * {@link #addDFAState} method also locks inside the DFA lock
 * but this time on the shared context cache when it rebuilds the
 * configurations' {@link PredictionContext} objects using cached
 * subgraphs/nodes. No other locking occurs, even during DFA simulation. This is
 * safe as long as we can guarantee that all threads referencing
 * {@code s.edge[t]} get the same physical target {@link DFAState}, or
 * {@code null}. Once into the DFA, the DFA simulation does not reference the
 * {@link DFA#states} map. It follows the {@link DFAState#edges} field to new
 * targets. The DFA simulator will either find {@link DFAState#edges} to be
 * {@code null}, to be non-{@code null} and {@code dfa.edges[t]} null, or
 * {@code dfa.edges[t]} to be non-null. The
 * {@link #addDFAEdge} method could be racing to set the field
 * but in either case the DFA simulator works; if {@code null}, and requests ATN
 * simulation. It could also race trying to get {@code dfa.edges[t]}, but either
 * way it will work because it's not doing a test and set operation.</p>
 *
 * <p>
 * <strong>Starting with SLL then failing to combined SLL/LL (Two-Stage
 * Parsing)</strong></p>
 *
 * <p>
 * Sam pointed out that if SLL does not give a syntax error, then there is no
 * point in doing full LL, which is slower. We only have to try LL if we get a
 * syntax error. For maximum speed, Sam starts the parser set to pure SLL
 * mode with the {@link BailErrorStrategy}:</p>
 *
 * <pre>
 * parser.{@link Parser#getInterpreter() getInterpreter()}.{@link
 * #setPredictionMode setPredictionMode}{@code (}{@link
 * PredictionMode#SLL}{@code )}; parser.{@link Parser#setErrorHandler
 * setErrorHandler}(new {@link BailErrorStrategy}());
 * </pre>
 *
 * <p>
 * If it does not get a syntax error, then we're done. If it does get a syntax
 * error, we need to retry with the combined SLL/LL strategy.</p>
 *
 * <p>
 * The reason this works is as follows. If there are no SLL conflicts, then the
 * grammar is SLL (at least for that input set). If there is an SLL conflict,
 * the full LL analysis must yield a set of viable alternatives which is a
 * subset of the alternatives reported by SLL. If the LL set is a singleton,
 * then the grammar is LL but not SLL. If the LL set is the same size as the SLL
 * set, the decision is SLL. If the LL set has size &gt; 1, then that decision
 * is truly ambiguous on the current input. If the LL set is smaller, then the
 * SLL conflict resolution might choose an alternative that the full LL would
 * rule out as a possibility based upon better context information. If that's
 * the case, then the SLL parse will definitely get an error because the full LL
 * analysis says it's not viable. If SLL conflict resolution chooses an
 * alternative within the LL set, them both SLL and LL would choose the same
 * alternative because they both choose the minimum of multiple conflicting
 * alternatives.</p>
 *
 * <p>
 * Let's say we have a set of SLL conflicting alternatives {@code {1, 2, 3}} and
 * a smaller LL set called <em>s</em>. If <em>s</em> is {@code {2, 3}}, then SLL
 * parsing will get an error because SLL will pursue alternative 1. If
 * <em>s</em> is {@code {1, 2}} or {@code {1, 3}} then both SLL and LL will
 * choose the same alternative because alternative one is the minimum of either
 * set. If <em>s</em> is {@code {2}} or {@code {3}} then SLL will get a syntax
 * error. If <em>s</em> is {@code {1}} then SLL will succeed.</p>
 *
 * <p>
 * Of course, if the input is invalid, then we will get an error for sure in
 * both SLL and LL parsing. Erroneous input will therefore require 2 passes over
 * the input.</p>
 */
class ANTLR4CPP_PUBLIC ParserATNSimulator : public ATNSimulator {
 public:
  /// Testing only!
  ParserATNSimulator(const ATN& atn, std::vector<dfa::DFA>& decisionToDFA,
                     PredictionContextCache& sharedContextCache);

  ParserATNSimulator(Parser* parser, const ATN& atn,
                     std::vector<dfa::DFA>& decisionToDFA,
                     PredictionContextCache& sharedContextCache);

  virtual void reset() override;
  virtual void clearDFA() override;
  virtual size_t adaptivePredict(TokenStream* input, size_t decision,
                                 ParserRuleContext* outerContext);

  static const bool TURN_OFF_LR_LOOP_ENTRY_BRANCH_OPT;

  std::vector<dfa::DFA>& decisionToDFA;

  /** Implements first-edge (loop entry) elimination as an optimization
   *  during closure operations.  See antlr/antlr4#1398.
   *
   * The optimization is to avoid adding the loop entry config when
   * the exit path can only lead back to the same
   * StarLoopEntryState after popping context at the rule end state
   * (traversing only epsilon edges, so we're still in closure, in
   * this same rule).
   *
   * We need to detect any state that can reach loop entry on
   * epsilon w/o exiting rule. We don't have to look at FOLLOW
   * links, just ensure that all stack tops for config refer to key
   * states in LR rule.
   *
   * To verify we are in the right situation we must first check
   * closure is at a StarLoopEntryState generated during LR removal.
   * Then we check that each stack top of context is a return state
   * from one of these cases:
   *
   *   1. 'not' expr, '(' type ')' expr. The return state points at loop entry
   * state 2. expr op expr. The return state is the block end of internal block
   * of (...)* 3. 'between' expr 'and' expr. The return state of 2nd expr
   * reference. That state points at block end of internal block of (...)*. 4.
   * expr '?' expr ':' expr. The return state points at block end, which
   * points at loop entry state.
   *
   * If any is true for each stack top, then closure does not add a
   * config to the current config set for edge[0], the loop entry branch.
   *
   *  Conditions fail if any context for the current config is:
   *
   *   a. empty (we'd fall out of expr to do a global FOLLOW which could
   *      even be to some weird spot in expr) or,
   *   b. lies outside of expr or,
   *   c. lies within expr but at a state not the BlockEndState
   *   generated during LR removal
   *
   * Do we need to evaluate predicates ever in closure for this case?
   *
   * No. Predicates, including precedence predicates, are only
   * evaluated when computing a DFA start state. I.e., only before
   * the lookahead (but not parser) consumes a token.
   *
   * There are no epsilon edges allowed in LR rule alt blocks or in
   * the "primary" part (ID here). If closure is in
   * StarLoopEntryState any lookahead operation will have consumed a
   * token as there are no epsilon-paths that lead to
   * StarLoopEntryState. We do not have to evaluate predicates
   * therefore if we are in the generated StarLoopEntryState of a LR
   * rule. Note that when making a prediction starting at that
   * decision point, decision d=2, compute-start-state performs
   * closure starting at edges[0], edges[1] emanating from
   * StarLoopEntryState. That means it is not performing closure on
   * StarLoopEntryState during compute-start-state.
   *
   * How do we know this always gives same prediction answer?
   *
   * Without predicates, loop entry and exit paths are ambiguous
   * upon remaining input +b (in, say, a+b). Either paths lead to
   * valid parses. Closure can lead to consuming + immediately or by
   * falling out of this call to expr back into expr and loop back
   * again to StarLoopEntryState to match +b. In this special case,
   * we choose the more efficient path, which is to take the bypass
   * path.
   *
   * The lookahead language has not changed because closure chooses
   * one path over the other. Both paths lead to consuming the same
   * remaining input during a lookahead operation. If the next token
   * is an operator, lookahead will enter the choice block with
   * operators. If it is not, lookahead will exit expr. Same as if
   * closure had chosen to enter the choice block immediately.
   *
   * Closure is examining one config (some loopentrystate, some alt,
   * context) which means it is considering exactly one alt. Closure
   * always copies the same alt to any derived configs.
   *
   * How do we know this optimization doesn't mess up precedence in
   * our parse trees?
   *
   * Looking through expr from left edge of stat only has to confirm
   * that an input, say, a+b+c; begins with any valid interpretation
   * of an expression. The precedence actually doesn't matter when
   * making a decision in stat seeing through expr. It is only when
   * parsing rule expr that we must use the precedence to get the
   * right interpretation and, hence, parse tree.
   */
  bool canDropLoopEntryEdgeInLeftRecursiveRule(ATNConfig* config) const;
  virtual std::string getRuleName(size_t index);

  virtual Ref<ATNConfig> precedenceTransition(Ref<ATNConfig> const& config,
                                              PrecedencePredicateTransition* pt,
                                              bool collectPredicates,
                                              bool inContext, bool fullCtx);

  void setPredictionMode(PredictionMode newMode);
  PredictionMode getPredictionMode();

  Parser* getParser();

  virtual std::string getTokenName(size_t t);

  virtual std::string getLookaheadName(TokenStream* input);

  /// <summary>
  /// Used for debugging in adaptivePredict around execATN but I cut
  ///  it out for clarity now that alg. works well. We can leave this
  ///  "dead" code for a bit.
  /// </summary>
  virtual void dumpDeadEndConfigs(NoViableAltException& nvae);

 protected:
  Parser* const parser;

  /// <summary>
  /// Each prediction operation uses a cache for merge of prediction contexts.
  /// Don't keep around as it wastes huge amounts of memory. The merge cache
  /// isn't synchronized but we're ok since two threads shouldn't reuse same
  /// parser/atnsim object because it can only handle one input at a time.
  /// This maps graphs a and b to merged result c. (a,b)->c. We can avoid
  /// the merge if we ever see a and b again.  Note that (b,a)->c should
  /// also be examined during cache lookup.
  /// </summary>
  PredictionContextMergeCache mergeCache;

  // LAME globals to avoid parameters!!!!! I need these down deep in
  // predTransition
  TokenStream* _input;
  size_t _startIndex;
  ParserRuleContext* _outerContext;
  dfa::DFA* _dfa;  // Reference into the decisionToDFA vector.

  /// <summary>
  /// Performs ATN simulation to compute a predicted alternative based
  ///  upon the remaining input, but also updates the DFA cache to avoid
  ///  having to traverse the ATN again for the same input sequence.
  ///
  /// There are some key conditions we're looking for after computing a new
  /// set of ATN configs (proposed DFA state):
  /// if the set is empty, there is no viable alternative for current symbol
  /// does the state uniquely predict an alternative?
  /// does the state have a conflict that would prevent us from
  ///         putting it on the work list?
  ///
  /// We also have some key operations to do:
  /// add an edge from previous DFA state to potentially new DFA state, D,
  ///         upon current symbol but only if adding to work list, which means
  ///         in all cases except no viable alternative (and possibly non-greedy
  ///         decisions?)
  /// collecting predicates and adding semantic context to DFA accept states
  /// adding rule context to context-sensitive DFA accept states
  /// consuming an input symbol
  /// reporting a conflict
  /// reporting an ambiguity
  /// reporting a context sensitivity
  /// reporting insufficient predicates
  ///
  /// cover these cases:
  ///    dead end
  ///    single alt
  ///    single alt + preds
  ///    conflict
  ///    conflict + preds
  /// </summary>
  virtual size_t execATN(dfa::DFA& dfa, dfa::DFAState* s0, TokenStream* input,
                         size_t startIndex, ParserRuleContext* outerContext);

  /// <summary>
  /// Get an existing target state for an edge in the DFA. If the target state
  /// for the edge has not yet been computed or is otherwise not available,
  /// this method returns {@code null}.
  /// </summary>
  /// <param name="previousD"> The current DFA state </param>
  /// <param name="t"> The next input symbol </param>
  /// <returns> The existing target DFA state for the given input symbol
  /// {@code t}, or {@code null} if the target state for this edge is not
  /// already cached </returns>
  virtual dfa::DFAState* getExistingTargetState(dfa::DFAState* previousD,
                                                size_t t);

  /// <summary>
  /// Compute a target state for an edge in the DFA, and attempt to add the
  /// computed state and corresponding edge to the DFA.
  /// </summary>
  /// <param name="dfa"> The DFA </param>
  /// <param name="previousD"> The current DFA state </param>
  /// <param name="t"> The next input symbol
  /// </param>
  /// <returns> The computed target DFA state for the given input symbol
  /// {@code t}. If {@code t} does not lead to a valid DFA state, this method
  /// returns <seealso cref="#ERROR"/>. </returns>
  virtual dfa::DFAState* computeTargetState(dfa::DFA& dfa,
                                            dfa::DFAState* previousD, size_t t);

  virtual void predicateDFAState(dfa::DFAState* dfaState,
                                 DecisionState* decisionState);

  // comes back with reach.uniqueAlt set to a valid alt
  virtual size_t execATNWithFullContext(
      dfa::DFA& dfa, dfa::DFAState* D, ATNConfigSet* s0, TokenStream* input,
      size_t startIndex,
      ParserRuleContext* outerContext);  // how far we got before failing over

  virtual std::unique_ptr<ATNConfigSet> computeReachSet(ATNConfigSet* closure,
                                                        size_t t, bool fullCtx);

  /// <summary>
  /// Return a configuration set containing only the configurations from
  /// {@code configs} which are in a <seealso cref="RuleStopState"/>. If all
  /// configurations in {@code configs} are already in a rule stop state, this
  /// method simply returns {@code configs}.
  /// <p/>
  /// When {@code lookToEndOfRule} is true, this method uses
  /// <seealso cref="ATN#nextTokens"/> for each configuration in {@code configs}
  /// which is not already in a rule stop state to see if a rule stop state is
  /// reachable from the configuration via epsilon-only transitions.
  /// </summary>
  /// <param name="configs"> the configuration set to update </param>
  /// <param name="lookToEndOfRule"> when true, this method checks for rule stop
  /// states reachable by epsilon-only transitions from each configuration in
  /// {@code configs}.
  /// </param>
  /// <returns> {@code configs} if all configurations in {@code configs} are in
  /// a rule stop state, otherwise return a new configuration set containing
  /// only the configurations from {@code configs} which are in a rule stop
  /// state </returns>
  virtual ATNConfigSet* removeAllConfigsNotInRuleStopState(
      ATNConfigSet* configs, bool lookToEndOfRule);

  virtual std::unique_ptr<ATNConfigSet> computeStartState(ATNState* p,
                                                          RuleContext* ctx,
                                                          bool fullCtx);

  /* parrt internal source braindump that doesn't mess up
   * external API spec.

   applyPrecedenceFilter is an optimization to avoid highly
   nonlinear prediction of expressions and other left recursive
   rules. The precedence predicates such as {3>=prec}? Are highly
   context-sensitive in that they can only be properly evaluated
   in the context of the proper prec argument. Without pruning,
   these predicates are normal predicates evaluated when we reach
   conflict state (or unique prediction). As we cannot evaluate
   these predicates out of context, the resulting conflict leads
   to full LL evaluation and nonlinear prediction which shows up
   very clearly with fairly large expressions.

   Example grammar:

   e : e '*' e
   | e '+' e
   | INT
   ;

   We convert that to the following:

   e[int prec]
   :   INT
   ( {3>=prec}? '*' e[4]
   | {2>=prec}? '+' e[3]
   )*
   ;

   The (..)* loop has a decision for the inner block as well as
   an enter or exit decision, which is what concerns us here. At
   the 1st + of input 1+2+3, the loop entry sees both predicates
   and the loop exit also sees both predicates by falling off the
   edge of e.  This is because we have no stack information with
   SLL and find the follow of e, which will hit the return states
   inside the loop after e[4] and e[3], which brings it back to
   the enter or exit decision. In this case, we know that we
   cannot evaluate those predicates because we have fallen off
   the edge of the stack and will in general not know which prec
   parameter is the right one to use in the predicate.

   Because we have special information, that these are precedence
   predicates, we can resolve them without failing over to full
   LL despite their context sensitive nature. We make an
   assumption that prec[-1] <= prec[0], meaning that the current
   precedence level is greater than or equal to the precedence
   level of recursive invocations above us in the stack. For
   example, if predicate {3>=prec}? is true of the current prec,
   then one option is to enter the loop to match it now. The
   other option is to exit the loop and the left recursive rule
   to match the current operator in rule invocation further up
   the stack. But, we know that all of those prec are lower or
   the same value and so we can decide to enter the loop instead
   of matching it later. That means we can strip out the other
   configuration for the exit branch.

   So imagine we have (14,1,$,{2>=prec}?) and then
   (14,2,$-dipsIntoOuterContext,{2>=prec}?). The optimization
   allows us to collapse these two configurations. We know that
   if {2>=prec}? is true for the current prec parameter, it will
   also be true for any prec from an invoking e call, indicated
   by dipsIntoOuterContext. As the predicates are both true, we
   have the option to evaluate them early in the decision start
   state. We do this by stripping both predicates and choosing to
   enter the loop as it is consistent with the notion of operator
   precedence. It's also how the full LL conflict resolution
   would work.

   The solution requires a different DFA start state for each
   precedence level.

   The basic filter mechanism is to remove configurations of the
   form (p, 2, pi) if (p, 1, pi) exists for the same p and pi. In
   other words, for the same ATN state and predicate context,
   remove any configuration associated with an exit branch if
   there is a configuration associated with the enter branch.

   It's also the case that the filter evaluates precedence
   predicates and resolves conflicts according to precedence
   levels. For example, for input 1+2+3 at the first +, we see
   prediction filtering

   [(11,1,[$],{3>=prec}?), (14,1,[$],{2>=prec}?), (5,2,[$],up=1),
   (11,2,[$],up=1),
   (14,2,[$],up=1)],hasSemanticContext=true,dipsIntoOuterContext

   to

   [(11,1,[$]), (14,1,[$]), (5,2,[$],up=1)],dipsIntoOuterContext

   This filters because {3>=prec}? evals to true and collapses
   (11,1,[$],{3>=prec}?) and (11,2,[$],up=1) since early conflict
   resolution based upon rules of operator precedence fits with
   our usual match first alt upon conflict.

   We noticed a problem where a recursive call resets precedence
   to 0. Sam's fix: each config has flag indicating if it has
   returned from an expr[0] call. then just don't filter any
   config with that flag set. flag is carried along in
   closure(). so to avoid adding field, set bit just under sign
   bit of dipsIntoOuterContext (SUPPRESS_PRECEDENCE_FILTER).
   With the change you filter "unless (p, 2, pi) was reached
   after leaving the rule stop state of the LR rule containing
   state p, corresponding to a rule invocation with precedence
   level 0"
   */

  /**
   * This method transforms the start state computed by
   * {@link #computeStartState} to the special start state used by a
   * precedence DFA for a particular precedence value. The transformation
   * process applies the following changes to the start state's configuration
   * set.
   *
   * <ol>
   * <li>Evaluate the precedence predicates for each configuration using
   * {@link SemanticContext#evalPrecedence}.</li>
   * <li>When {@link ATNConfig#isPrecedenceFilterSuppressed} is {@code false},
   * remove all configurations which predict an alternative greater than 1,
   * for which another configuration that predicts alternative 1 is in the
   * same ATN state with the same prediction context. This transformation is
   * valid for the following reasons:
   * <ul>
   * <li>The closure block cannot contain any epsilon transitions which bypass
   * the body of the closure, so all states reachable via alternative 1 are
   * part of the precedence alternatives of the transformed left-recursive
   * rule.</li>
   * <li>The "primary" portion of a left recursive rule cannot contain an
   * epsilon transition, so the only way an alternative other than 1 can exist
   * in a state that is also reachable via alternative 1 is by nesting calls
   * to the left-recursive rule, with the outer calls not being at the
   * preferred precedence level. The
   * {@link ATNConfig#isPrecedenceFilterSuppressed} property marks ATN
   * configurations which do not meet this condition, and therefore are not
   * eligible for elimination during the filtering process.</li>
   * </ul>
   * </li>
   * </ol>
   *
   * <p>
   * The prediction context must be considered by this filter to address
   * situations like the following.
   * </p>
   * <code>
   * <pre>
   * grammar TA;
   * prog: statement* EOF;
   * statement: letterA | statement letterA 'b' ;
   * letterA: 'a';
   * </pre>
   * </code>
   * <p>
   * If the above grammar, the ATN state immediately before the token
   * reference {@code 'a'} in {@code letterA} is reachable from the left edge
   * of both the primary and closure blocks of the left-recursive rule
   * {@code statement}. The prediction context associated with each of these
   * configurations distinguishes between them, and prevents the alternative
   * which stepped out to {@code prog} (and then back in to {@code statement}
   * from being eliminated by the filter.
   * </p>
   *
   * @param configs The configuration set computed by
   * {@link #computeStartState} as the start state for the DFA.
   * @return The transformed configuration set representing the start state
   * for a precedence DFA at a particular precedence level (determined by
   * calling {@link Parser#getPrecedence}).
   */
  std::unique_ptr<ATNConfigSet> applyPrecedenceFilter(ATNConfigSet* configs);

  virtual ATNState* getReachableTarget(Transition* trans, size_t ttype);

  virtual std::vector<Ref<SemanticContext>> getPredsForAmbigAlts(
      const antlrcpp::BitSet& ambigAlts, ATNConfigSet* configs, size_t nalts);

  virtual std::vector<dfa::DFAState::PredPrediction*> getPredicatePredictions(
      const antlrcpp::BitSet& ambigAlts,
      std::vector<Ref<SemanticContext>> altToPred);

  /**
   * This method is used to improve the localization of error messages by
   * choosing an alternative rather than throwing a
   * {@link NoViableAltException} in particular prediction scenarios where the
   * {@link #ERROR} state was reached during ATN simulation.
   *
   * <p>
   * The default implementation of this method uses the following
   * algorithm to identify an ATN configuration which successfully parsed the
   * decision entry rule. Choosing such an alternative ensures that the
   * {@link ParserRuleContext} returned by the calling rule will be complete
   * and valid, and the syntax error will be reported later at a more
   * localized location.</p>
   *
   * <ul>
   * <li>If a syntactically valid path or paths reach the end of the decision
   * rule and they are semantically valid if predicated, return the min
   * associated alt.</li> <li>Else, if a semantically invalid but syntactically
   * valid path exist or paths exist, return the minimum associated alt.
   * </li>
   * <li>Otherwise, return {@link ATN#INVALID_ALT_NUMBER}.</li>
   * </ul>
   *
   * <p>
   * In some scenarios, the algorithm described above could predict an
   * alternative which will result in a {@link FailedPredicateException} in
   * the parser. Specifically, this could occur if the <em>only</em>
   * configuration capable of successfully parsing to the end of the decision
   * rule is blocked by a semantic predicate. By choosing this alternative
   * within
   * {@link #adaptivePredict} instead of throwing a
   * {@link NoViableAltException}, the resulting
   * {@link FailedPredicateException} in the parser will identify the specific
   * predicate which is preventing the parser from successfully parsing the
   * decision rule, which helps developers identify and correct logic errors
   * in semantic predicates.
   * </p>
   *
   * @param configs The ATN configurations which were valid immediately before
   * the {@link #ERROR} state was reached
   * @param outerContext The is the \gamma_0 initial parser context from the
   * paper or the parser stack at the instant before prediction commences.
   *
   * @return The value to return from {@link #adaptivePredict}, or
   * {@link ATN#INVALID_ALT_NUMBER} if a suitable alternative was not
   * identified and {@link #adaptivePredict} should report an error instead.
   */
  size_t getSynValidOrSemInvalidAltThatFinishedDecisionEntryRule(
      ATNConfigSet* configs, ParserRuleContext* outerContext);

  virtual size_t getAltThatFinishedDecisionEntryRule(ATNConfigSet* configs);

  /** Walk the list of configurations and split them according to
   *  those that have preds evaluating to true/false.  If no pred, assume
   *  true pred and include in succeeded set.  Returns Pair of sets.
   *
   *  Create a new set so as not to alter the incoming parameter.
   *
   *  Assumption: the input stream has been restored to the starting point
   *  prediction, which is where predicates need to evaluate.
   */
  std::pair<ATNConfigSet*, ATNConfigSet*> splitAccordingToSemanticValidity(
      ATNConfigSet* configs, ParserRuleContext* outerContext);

  /// <summary>
  /// Look through a list of predicate/alt pairs, returning alts for the
  ///  pairs that win. A {@code NONE} predicate indicates an alt containing an
  ///  unpredicated config which behaves as "always true." If !complete
  ///  then we stop at the first predicate that evaluates to true. This
  ///  includes pairs with null predicates.
  /// </summary>
  virtual antlrcpp::BitSet evalSemanticContext(
      std::vector<dfa::DFAState::PredPrediction*> predPredictions,
      ParserRuleContext* outerContext, bool complete);

  /**
   * Evaluate a semantic context within a specific parser context.
   *
   * <p>
   * This method might not be called for every semantic context evaluated
   * during the prediction process. In particular, we currently do not
   * evaluate the following but it may change in the future:</p>
   *
   * <ul>
   * <li>Precedence predicates (represented by
   * {@link SemanticContext.PrecedencePredicate}) are not currently evaluated
   * through this method.</li>
   * <li>Operator predicates (represented by {@link SemanticContext.AND} and
   * {@link SemanticContext.OR}) are evaluated as a single semantic
   * context, rather than evaluating the operands individually.
   * Implementations which require evaluation results from individual
   * predicates should override this method to explicitly handle evaluation of
   * the operands within operator predicates.</li>
   * </ul>
   *
   * @param pred The semantic context to evaluate
   * @param parserCallStack The parser context in which to evaluate the
   * semantic context
   * @param alt The alternative which is guarded by {@code pred}
   * @param fullCtx {@code true} if the evaluation is occurring during LL
   * prediction; otherwise, {@code false} if the evaluation is occurring
   * during SLL prediction
   *
   * @since 4.3
   */
  virtual bool evalSemanticContext(Ref<SemanticContext> const& pred,
                                   ParserRuleContext* parserCallStack,
                                   size_t alt, bool fullCtx);

  /* TO_DO: If we are doing predicates, there is no point in pursuing
   closure operations if we reach a DFA state that uniquely predicts
   alternative. We will not be caching that DFA state and it is a
   waste to pursue the closure. Might have to advance when we do
   ambig detection thought :(
   */
  virtual void closure(Ref<ATNConfig> const& config, ATNConfigSet* configs,
                       ATNConfig::Set& closureBusy, bool collectPredicates,
                       bool fullCtx, bool treatEofAsEpsilon);

  virtual void closureCheckingStopState(Ref<ATNConfig> const& config,
                                        ATNConfigSet* configs,
                                        ATNConfig::Set& closureBusy,
                                        bool collectPredicates, bool fullCtx,
                                        int depth, bool treatEofAsEpsilon);

  /// Do the actual work of walking epsilon edges.
  virtual void closure_(Ref<ATNConfig> const& config, ATNConfigSet* configs,
                        ATNConfig::Set& closureBusy, bool collectPredicates,
                        bool fullCtx, int depth, bool treatEofAsEpsilon);

  virtual Ref<ATNConfig> getEpsilonTarget(Ref<ATNConfig> const& config,
                                          Transition* t, bool collectPredicates,
                                          bool inContext, bool fullCtx,
                                          bool treatEofAsEpsilon);
  virtual Ref<ATNConfig> actionTransition(Ref<ATNConfig> const& config,
                                          ActionTransition* t);

  virtual Ref<ATNConfig> predTransition(Ref<ATNConfig> const& config,
                                        PredicateTransition* pt,
                                        bool collectPredicates, bool inContext,
                                        bool fullCtx);

  virtual Ref<ATNConfig> ruleTransition(Ref<ATNConfig> const& config,
                                        RuleTransition* t);

  /**
   * Gets a {@link BitSet} containing the alternatives in {@code configs}
   * which are part of one or more conflicting alternative subsets.
   *
   * @param configs The {@link ATNConfigSet} to analyze.
   * @return The alternatives in {@code configs} which are part of one or more
   * conflicting alternative subsets. If {@code configs} does not contain any
   * conflicting subsets, this method returns an empty {@link BitSet}.
   */
  virtual antlrcpp::BitSet getConflictingAlts(ATNConfigSet* configs);

  /// <summary>
  /// Sam pointed out a problem with the previous definition, v3, of
  /// ambiguous states. If we have another state associated with conflicting
  /// alternatives, we should keep going. For example, the following grammar
  ///
  /// s : (ID | ID ID?) ';' ;
  ///
  /// When the ATN simulation reaches the state before ';', it has a DFA
  /// state that looks like: [12|1|[], 6|2|[], 12|2|[]]. Naturally
  /// 12|1|[] and 12|2|[] conflict, but we cannot stop processing this node
  /// because alternative to has another way to continue, via [6|2|[]].
  /// The key is that we have a single state that has config's only associated
  /// with a single alternative, 2, and crucially the state transitions
  /// among the configurations are all non-epsilon transitions. That means
  /// we don't consider any conflicts that include alternative 2. So, we
  /// ignore the conflict between alts 1 and 2. We ignore a set of
  /// conflicting alts when there is an intersection with an alternative
  /// associated with a single alt state in the state->config-list map.
  ///
  /// It's also the case that we might have two conflicting configurations but
  /// also a 3rd nonconflicting configuration for a different alternative:
  /// [1|1|[], 1|2|[], 8|3|[]]. This can come about from grammar:
  ///
  /// a : A | A | A B ;
  ///
  /// After matching input A, we reach the stop state for rule A, state 1.
  /// State 8 is the state right before B. Clearly alternatives 1 and 2
  /// conflict and no amount of further lookahead will separate the two.
  /// However, alternative 3 will be able to continue and so we do not
  /// stop working on this state. In the previous example, we're concerned
  /// with states associated with the conflicting alternatives. Here alt
  /// 3 is not associated with the conflicting configs, but since we can
  /// continue looking for input reasonably, I don't declare the state done. We
  /// ignore a set of conflicting alts when we have an alternative
  /// that we still need to pursue.
  /// </summary>

  virtual antlrcpp::BitSet getConflictingAltsOrUniqueAlt(ATNConfigSet* configs);

  virtual NoViableAltException noViableAlt(TokenStream* input,
                                           ParserRuleContext* outerContext,
                                           ATNConfigSet* configs,
                                           size_t startIndex);

  static size_t getUniqueAlt(ATNConfigSet* configs);

  /// <summary>
  /// Add an edge to the DFA, if possible. This method calls
  /// <seealso cref="#addDFAState"/> to ensure the {@code to} state is present
  /// in the DFA. If {@code from} is {@code null}, or if {@code t} is outside
  /// the range of edges that can be represented in the DFA tables, this method
  /// returns without adding the edge to the DFA.
  /// <p/>
  /// If {@code to} is {@code null}, this method returns {@code null}.
  /// Otherwise, this method returns the <seealso cref="DFAState"/> returned by
  /// calling <seealso cref="#addDFAState"/> for the {@code to} state.
  /// </summary>
  /// <param name="dfa"> The DFA </param>
  /// <param name="from"> The source state for the edge </param>
  /// <param name="t"> The input symbol </param>
  /// <param name="to"> The target state for the edge
  /// </param>
  /// <returns> If {@code to} is {@code null}, this method returns {@code null};
  /// otherwise this method returns the result of calling <seealso
  /// cref="#addDFAState"/> on {@code to} </returns>
  virtual dfa::DFAState* addDFAEdge(dfa::DFA& dfa, dfa::DFAState* from,
                                    ssize_t t, dfa::DFAState* to);

  /// <summary>
  /// Add state {@code D} to the DFA if it is not already present, and return
  /// the actual instance stored in the DFA. If a state equivalent to {@code D}
  /// is already in the DFA, the existing state is returned. Otherwise this
  /// method returns {@code D} after adding it to the DFA.
  /// <p/>
  /// If {@code D} is <seealso cref="#ERROR"/>, this method returns <seealso
  /// cref="#ERROR"/> and does not change the DFA.
  /// </summary>
  /// <param name="dfa"> The dfa </param>
  /// <param name="D"> The DFA state to add </param>
  /// <returns> The state stored in the DFA. This will be either the existing
  /// state if {@code D} is already in the DFA, or {@code D} itself if the
  /// state was not already present. </returns>
  virtual dfa::DFAState* addDFAState(dfa::DFA& dfa, dfa::DFAState* D);

  virtual void reportAttemptingFullContext(
      dfa::DFA& dfa, const antlrcpp::BitSet& conflictingAlts,
      ATNConfigSet* configs, size_t startIndex, size_t stopIndex);

  virtual void reportContextSensitivity(dfa::DFA& dfa, size_t prediction,
                                        ATNConfigSet* configs,
                                        size_t startIndex, size_t stopIndex);

  /// If context sensitive parsing, we know it's ambiguity not conflict.
  virtual void reportAmbiguity(
      dfa::DFA& dfa,
      dfa::DFAState* D,  // the DFA state from execATN() that had SLL conflicts
      size_t startIndex, size_t stopIndex, bool exact,
      const antlrcpp::BitSet& ambigAlts,
      ATNConfigSet* configs);  // configs that LL not SLL considered conflicting

 private:
  // SLL, LL, or LL + exact ambig detection?
  PredictionMode _mode;

  static bool getLrLoopSetting();
  void InitializeInstanceFields();
};

}  // namespace atn
}  // namespace antlr4
