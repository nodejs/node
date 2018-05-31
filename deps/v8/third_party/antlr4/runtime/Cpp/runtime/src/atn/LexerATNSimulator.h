/* Copyright (c) 2012-2017 The ANTLR Project. All rights reserved.
 * Use of this file is governed by the BSD 3-clause license that
 * can be found in the LICENSE.txt file in the project root.
 */

#pragma once

#include "atn/ATNConfigSet.h"
#include "atn/ATNSimulator.h"
#include "atn/LexerATNConfig.h"

namespace antlr4 {
namespace atn {

/// "dup" of ParserInterpreter
class ANTLR4CPP_PUBLIC LexerATNSimulator : public ATNSimulator {
 protected:
  class SimState {
   public:
    virtual ~SimState();

   protected:
    size_t index;
    size_t line;
    size_t charPos;
    dfa::DFAState* dfaState;
    virtual void reset();
    friend class LexerATNSimulator;

   private:
    void InitializeInstanceFields();

   public:
    SimState() { InitializeInstanceFields(); }
  };

 public:
  static const size_t MIN_DFA_EDGE = 0;
  static const size_t MAX_DFA_EDGE = 127;  // forces unicode to stay in ATN

 protected:
  /// <summary>
  /// When we hit an accept state in either the DFA or the ATN, we
  ///  have to notify the character stream to start buffering characters
  ///  via <seealso cref="IntStream#mark"/> and record the current state. The
  ///  current sim state includes the current index into the input, the current
  ///  line, and current character position in that line. Note that the Lexer is
  ///  tracking the starting line and characterization of the token. These
  ///  variables track the "state" of the simulator when it hits an accept
  ///  state.
  /// <p/>
  ///  We track these variables separately for the DFA and ATN simulation
  ///  because the DFA simulation often has to fail over to the ATN
  ///  simulation. If the ATN simulation fails, we need the DFA to fall
  ///  back to its previously accepted state, if any. If the ATN succeeds,
  ///  then the ATN does the accept and the DFA simulator that invoked it
  ///  can simply return the predicted token type.
  /// </summary>
  Lexer* const _recog;

  /// The current token's starting index into the character stream.
  ///  Shared across DFA to ATN simulation in case the ATN fails and the
  ///  DFA did not have a previous accept state. In this case, we use the
  ///  ATN-generated exception object.
  size_t _startIndex;

  /// line number 1..n within the input.
  size_t _line;

  /// The index of the character relative to the beginning of the line 0..n-1.
  size_t _charPositionInLine;

 public:
  std::vector<dfa::DFA>& _decisionToDFA;

 protected:
  size_t _mode;

  /// Used during DFA/ATN exec to record the most recent accept configuration
  /// info.
  SimState _prevAccept;

 public:
  static int match_calls;

  LexerATNSimulator(const ATN& atn, std::vector<dfa::DFA>& decisionToDFA,
                    PredictionContextCache& sharedContextCache);
  LexerATNSimulator(Lexer* recog, const ATN& atn,
                    std::vector<dfa::DFA>& decisionToDFA,
                    PredictionContextCache& sharedContextCache);
  virtual ~LexerATNSimulator() {}

  virtual void copyState(LexerATNSimulator* simulator);
  virtual size_t match(CharStream* input, size_t mode);
  virtual void reset() override;

  virtual void clearDFA() override;

 protected:
  virtual size_t matchATN(CharStream* input);
  virtual size_t execATN(CharStream* input, dfa::DFAState* ds0);

  /// <summary>
  /// Get an existing target state for an edge in the DFA. If the target state
  /// for the edge has not yet been computed or is otherwise not available,
  /// this method returns {@code null}.
  /// </summary>
  /// <param name="s"> The current DFA state </param>
  /// <param name="t"> The next input symbol </param>
  /// <returns> The existing target DFA state for the given input symbol
  /// {@code t}, or {@code null} if the target state for this edge is not
  /// already cached </returns>
  virtual dfa::DFAState* getExistingTargetState(dfa::DFAState* s, size_t t);

  /// <summary>
  /// Compute a target state for an edge in the DFA, and attempt to add the
  /// computed state and corresponding edge to the DFA.
  /// </summary>
  /// <param name="input"> The input stream </param>
  /// <param name="s"> The current DFA state </param>
  /// <param name="t"> The next input symbol
  /// </param>
  /// <returns> The computed target DFA state for the given input symbol
  /// {@code t}. If {@code t} does not lead to a valid DFA state, this method
  /// returns <seealso cref="#ERROR"/>. </returns>
  virtual dfa::DFAState* computeTargetState(CharStream* input, dfa::DFAState* s,
                                            size_t t);

  virtual size_t failOrAccept(CharStream* input, ATNConfigSet* reach, size_t t);

  /// <summary>
  /// Given a starting configuration set, figure out all ATN configurations
  ///  we can reach upon input {@code t}. Parameter {@code reach} is a return
  ///  parameter.
  /// </summary>
  void getReachableConfigSet(
      CharStream* input,
      ATNConfigSet* closure_,  // closure_ as we have a closure() already
      ATNConfigSet* reach, size_t t);

  virtual void accept(CharStream* input,
                      const Ref<LexerActionExecutor>& lexerActionExecutor,
                      size_t startIndex, size_t index, size_t line,
                      size_t charPos);

  virtual ATNState* getReachableTarget(Transition* trans, size_t t);

  virtual std::unique_ptr<ATNConfigSet> computeStartState(CharStream* input,
                                                          ATNState* p);

  /// <summary>
  /// Since the alternatives within any lexer decision are ordered by
  /// preference, this method stops pursuing the closure as soon as an accept
  /// state is reached. After the first accept state is reached by depth-first
  /// search from {@code config}, all other (potentially reachable) states for
  /// this rule would have a lower priority.
  /// </summary>
  /// <returns> {@code true} if an accept state is reached, otherwise
  /// {@code false}. </returns>
  virtual bool closure(CharStream* input, const Ref<LexerATNConfig>& config,
                       ATNConfigSet* configs, bool currentAltReachedAcceptState,
                       bool speculative, bool treatEofAsEpsilon);

  // side-effect: can alter configs.hasSemanticContext
  virtual Ref<LexerATNConfig> getEpsilonTarget(
      CharStream* input, const Ref<LexerATNConfig>& config, Transition* t,
      ATNConfigSet* configs, bool speculative, bool treatEofAsEpsilon);

  /// <summary>
  /// Evaluate a predicate specified in the lexer.
  /// <p/>
  /// If {@code speculative} is {@code true}, this method was called before
  /// <seealso cref="#consume"/> for the matched character. This method should
  /// call <seealso cref="#consume"/> before evaluating the predicate to ensure
  /// position sensitive values, including <seealso cref="Lexer#getText"/>,
  /// <seealso cref="Lexer#getLine"/>, and <seealso
  /// cref="Lexer#getCharPositionInLine"/>, properly reflect the current lexer
  /// state. This method should restore {@code input} and the simulator to the
  /// original state before returning (i.e. undo the actions made by the call to
  /// <seealso cref="#consume"/>.
  /// </summary>
  /// <param name="input"> The input stream. </param>
  /// <param name="ruleIndex"> The rule containing the predicate. </param>
  /// <param name="predIndex"> The index of the predicate within the rule.
  /// </param> <param name="speculative"> {@code true} if the current index in
  /// {@code input} is one character before the predicate's location.
  /// </param>
  /// <returns> {@code true} if the specified predicate evaluates to
  /// {@code true}. </returns>
  virtual bool evaluatePredicate(CharStream* input, size_t ruleIndex,
                                 size_t predIndex, bool speculative);

  virtual void captureSimState(CharStream* input, dfa::DFAState* dfaState);
  virtual dfa::DFAState* addDFAEdge(dfa::DFAState* from, size_t t,
                                    ATNConfigSet* q);
  virtual void addDFAEdge(dfa::DFAState* p, size_t t, dfa::DFAState* q);

  /// <summary>
  /// Add a new DFA state if there isn't one with this set of
  /// configurations already. This method also detects the first
  /// configuration containing an ATN rule stop state. Later, when
  /// traversing the DFA, we will know which rule to accept.
  /// </summary>
  virtual dfa::DFAState* addDFAState(ATNConfigSet* configs);

 public:
  dfa::DFA& getDFA(size_t mode);

  /// Get the text matched so far for the current token.
  virtual std::string getText(CharStream* input);
  virtual size_t getLine() const;
  virtual void setLine(size_t line);
  virtual size_t getCharPositionInLine();
  virtual void setCharPositionInLine(size_t charPositionInLine);
  virtual void consume(CharStream* input);
  virtual std::string getTokenName(size_t t);

 private:
  void InitializeInstanceFields();
};

}  // namespace atn
}  // namespace antlr4
