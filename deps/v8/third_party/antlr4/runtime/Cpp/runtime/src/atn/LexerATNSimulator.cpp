/* Copyright (c) 2012-2017 The ANTLR Project. All rights reserved.
 * Use of this file is governed by the BSD 3-clause license that
 * can be found in the LICENSE.txt file in the project root.
 */

#include "IntStream.h"
#include "Lexer.h"
#include "LexerNoViableAltException.h"
#include "Token.h"
#include "atn/ActionTransition.h"
#include "atn/OrderedATNConfigSet.h"
#include "atn/PredicateTransition.h"
#include "atn/RuleStopState.h"
#include "atn/RuleTransition.h"
#include "atn/SingletonPredictionContext.h"
#include "atn/TokensStartState.h"
#include "dfa/DFA.h"
#include "misc/Interval.h"

#include "atn/EmptyPredictionContext.h"
#include "atn/LexerATNConfig.h"
#include "atn/LexerActionExecutor.h"
#include "dfa/DFAState.h"

#include "atn/LexerATNSimulator.h"

#define DEBUG_ATN 0
#define DEBUG_DFA 0

using namespace antlr4;
using namespace antlr4::atn;
using namespace antlrcpp;

LexerATNSimulator::SimState::~SimState() {}

void LexerATNSimulator::SimState::reset() {
  index = INVALID_INDEX;
  line = 0;
  charPos = INVALID_INDEX;
  dfaState = nullptr;  // Don't delete. It's just a reference.
}

void LexerATNSimulator::SimState::InitializeInstanceFields() {
  index = INVALID_INDEX;
  line = 0;
  charPos = INVALID_INDEX;
}

int LexerATNSimulator::match_calls = 0;

LexerATNSimulator::LexerATNSimulator(const ATN& atn,
                                     std::vector<dfa::DFA>& decisionToDFA,
                                     PredictionContextCache& sharedContextCache)
    : LexerATNSimulator(nullptr, atn, decisionToDFA, sharedContextCache) {}

LexerATNSimulator::LexerATNSimulator(Lexer* recog, const ATN& atn,
                                     std::vector<dfa::DFA>& decisionToDFA,
                                     PredictionContextCache& sharedContextCache)
    : ATNSimulator(atn, sharedContextCache),
      _recog(recog),
      _decisionToDFA(decisionToDFA) {
  InitializeInstanceFields();
}

void LexerATNSimulator::copyState(LexerATNSimulator* simulator) {
  _charPositionInLine = simulator->_charPositionInLine;
  _line = simulator->_line;
  _mode = simulator->_mode;
  _startIndex = simulator->_startIndex;
}

size_t LexerATNSimulator::match(CharStream* input, size_t mode) {
  match_calls++;
  _mode = mode;
  ssize_t mark = input->mark();

  auto onExit = finally([input, mark] { input->release(mark); });

  _startIndex = input->index();
  _prevAccept.reset();
  const dfa::DFA& dfa = _decisionToDFA[mode];
  if (dfa.s0 == nullptr) {
    return matchATN(input);
  } else {
    return execATN(input, dfa.s0);
  }
}

void LexerATNSimulator::reset() {
  _prevAccept.reset();
  _startIndex = 0;
  _line = 1;
  _charPositionInLine = 0;
  _mode = Lexer::DEFAULT_MODE;
}

void LexerATNSimulator::clearDFA() {
  size_t size = _decisionToDFA.size();
  _decisionToDFA.clear();
  for (size_t d = 0; d < size; ++d) {
    _decisionToDFA.emplace_back(atn.getDecisionState(d), d);
  }
}

size_t LexerATNSimulator::matchATN(CharStream* input) {
  ATNState* startState = atn.modeToStartState[_mode];

  std::unique_ptr<ATNConfigSet> s0_closure =
      computeStartState(input, startState);

  bool suppressEdge = s0_closure->hasSemanticContext;
  s0_closure->hasSemanticContext = false;

  dfa::DFAState* next = addDFAState(s0_closure.release());
  if (!suppressEdge) {
    _decisionToDFA[_mode].s0 = next;
  }

  size_t predict = execATN(input, next);

  return predict;
}

size_t LexerATNSimulator::execATN(CharStream* input, dfa::DFAState* ds0) {
  if (ds0->isAcceptState) {
    // allow zero-length tokens
    // ml: in Java code this method uses 3 params. The first is a member var of
    // the class anyway (_prevAccept), so why pass it here?
    captureSimState(input, ds0);
  }

  size_t t = input->LA(1);
  dfa::DFAState* s = ds0;  // s is current/from DFA state

  while (true) {  // while more work
    // As we move src->trg, src->trg, we keep track of the previous trg to
    // avoid looking up the DFA state again, which is expensive.
    // If the previous target was already part of the DFA, we might
    // be able to avoid doing a reach operation upon t. If s!=null,
    // it means that semantic predicates didn't prevent us from
    // creating a DFA state. Once we know s!=null, we check to see if
    // the DFA state has an edge already for t. If so, we can just reuse
    // it's configuration set; there's no point in re-computing it.
    // This is kind of like doing DFA simulation within the ATN
    // simulation because DFA simulation is really just a way to avoid
    // computing reach/closure sets. Technically, once we know that
    // we have a previously added DFA state, we could jump over to
    // the DFA simulator. But, that would mean popping back and forth
    // a lot and making things more complicated algorithmically.
    // This optimization makes a lot of sense for loops within DFA.
    // A character will take us back to an existing DFA state
    // that already has lots of edges out of it. e.g., .* in comments.
    dfa::DFAState* target = getExistingTargetState(s, t);
    if (target == nullptr) {
      target = computeTargetState(input, s, t);
    }

    if (target == ERROR_STATE.get()) {
      break;
    }

    // If this is a consumable input element, make sure to consume before
    // capturing the accept state so the input index, line, and char
    // position accurately reflect the state of the interpreter at the
    // end of the token.
    if (t != Token::EOF) {
      consume(input);
    }

    if (target->isAcceptState) {
      captureSimState(input, target);
      if (t == Token::EOF) {
        break;
      }
    }

    t = input->LA(1);
    s = target;  // flip; current DFA target becomes new src/from state
  }

  return failOrAccept(input, s->configs.get(), t);
}

dfa::DFAState* LexerATNSimulator::getExistingTargetState(dfa::DFAState* s,
                                                         size_t t) {
  dfa::DFAState* retval = nullptr;
  _edgeLock.readLock();
  if (t <= MAX_DFA_EDGE) {
    auto iterator = s->edges.find(t - MIN_DFA_EDGE);
#if DEBUG_ATN == 1
    if (iterator != s->edges.end()) {
      std::cout << std::string("reuse state ") << s->stateNumber
                << std::string(" edge to ") << iterator->second->stateNumber
                << std::endl;
    }
#endif

    if (iterator != s->edges.end()) retval = iterator->second;
  }
  _edgeLock.readUnlock();
  return retval;
}

dfa::DFAState* LexerATNSimulator::computeTargetState(CharStream* input,
                                                     dfa::DFAState* s,
                                                     size_t t) {
  OrderedATNConfigSet* reach =
      new OrderedATNConfigSet(); /* mem-check: deleted on error or managed by
                                    new DFA state. */

  // if we don't find an existing DFA state
  // Fill reach starting from closure, following t transitions
  getReachableConfigSet(input, s->configs.get(), reach, t);

  if (reach->isEmpty()) {  // we got nowhere on t from s
    if (!reach->hasSemanticContext) {
      // we got nowhere on t, don't throw out this knowledge; it'd
      // cause a failover from DFA later.
      delete reach;
      addDFAEdge(s, t, ERROR_STATE.get());
    }

    // stop when we can't match any more char
    return ERROR_STATE.get();
  }

  // Add an edge from s to target DFA found/created for reach
  return addDFAEdge(s, t, reach);
}

size_t LexerATNSimulator::failOrAccept(CharStream* input, ATNConfigSet* reach,
                                       size_t t) {
  if (_prevAccept.dfaState != nullptr) {
    Ref<LexerActionExecutor> lexerActionExecutor =
        _prevAccept.dfaState->lexerActionExecutor;
    accept(input, lexerActionExecutor, _startIndex, _prevAccept.index,
           _prevAccept.line, _prevAccept.charPos);
    return _prevAccept.dfaState->prediction;
  } else {
    // if no accept and EOF is first char, return EOF
    if (t == Token::EOF && input->index() == _startIndex) {
      return Token::EOF;
    }

    throw LexerNoViableAltException(_recog, input, _startIndex, reach);
  }
}

void LexerATNSimulator::getReachableConfigSet(CharStream* input,
                                              ATNConfigSet* closure_,
                                              ATNConfigSet* reach, size_t t) {
  // this is used to skip processing for configs which have a lower priority
  // than a config that already reached an accept state for the same rule
  size_t skipAlt = ATN::INVALID_ALT_NUMBER;

  for (auto c : closure_->configs) {
    bool currentAltReachedAcceptState = c->alt == skipAlt;
    if (currentAltReachedAcceptState &&
        (std::static_pointer_cast<LexerATNConfig>(c))
            ->hasPassedThroughNonGreedyDecision()) {
      continue;
    }

#if DEBUG_ATN == 1
    std::cout << "testing " << getTokenName((int)t) << " at "
              << c->toString(true) << std::endl;
#endif

    size_t n = c->state->transitions.size();
    for (size_t ti = 0; ti < n; ti++) {  // for each transition
      Transition* trans = c->state->transitions[ti];
      ATNState* target = getReachableTarget(trans, (int)t);
      if (target != nullptr) {
        Ref<LexerActionExecutor> lexerActionExecutor =
            std::static_pointer_cast<LexerATNConfig>(c)
                ->getLexerActionExecutor();
        if (lexerActionExecutor != nullptr) {
          lexerActionExecutor = lexerActionExecutor->fixOffsetBeforeMatch(
              (int)input->index() - (int)_startIndex, lexerActionExecutor);
        }

        bool treatEofAsEpsilon = t == Token::EOF;
        Ref<LexerATNConfig> config = std::make_shared<LexerATNConfig>(
            std::static_pointer_cast<LexerATNConfig>(c), target,
            lexerActionExecutor);

        if (closure(input, config, reach, currentAltReachedAcceptState, true,
                    treatEofAsEpsilon)) {
          // any remaining configs for this alt have a lower priority than
          // the one that just reached an accept state.
          skipAlt = c->alt;
          break;
        }
      }
    }
  }
}

void LexerATNSimulator::accept(
    CharStream* input, const Ref<LexerActionExecutor>& lexerActionExecutor,
    size_t /*startIndex*/, size_t index, size_t line, size_t charPos) {
#if DEBUG_ATN == 1
  std::cout << "ACTION ";
  std::cout << toString(lexerActionExecutor) << std::endl;
#endif

  // seek to after last char in token
  input->seek(index);
  _line = line;
  _charPositionInLine = (int)charPos;

  if (lexerActionExecutor != nullptr && _recog != nullptr) {
    lexerActionExecutor->execute(_recog, input, _startIndex);
  }
}

atn::ATNState* LexerATNSimulator::getReachableTarget(Transition* trans,
                                                     size_t t) {
  if (trans->matches(t, Lexer::MIN_CHAR_VALUE, Lexer::MAX_CHAR_VALUE)) {
    return trans->target;
  }

  return nullptr;
}

std::unique_ptr<ATNConfigSet> LexerATNSimulator::computeStartState(
    CharStream* input, ATNState* p) {
  Ref<PredictionContext> initialContext =
      PredictionContext::EMPTY;  // ml: the purpose of this assignment is
                                 // unclear
  std::unique_ptr<ATNConfigSet> configs(new OrderedATNConfigSet());
  for (size_t i = 0; i < p->transitions.size(); i++) {
    ATNState* target = p->transitions[i]->target;
    Ref<LexerATNConfig> c =
        std::make_shared<LexerATNConfig>(target, (int)(i + 1), initialContext);
    closure(input, c, configs.get(), false, false, false);
  }

  return configs;
}

bool LexerATNSimulator::closure(CharStream* input,
                                const Ref<LexerATNConfig>& config,
                                ATNConfigSet* configs,
                                bool currentAltReachedAcceptState,
                                bool speculative, bool treatEofAsEpsilon) {
#if DEBUG_ATN == 1
  std::cout << "closure(" << config->toString(true) << ")" << std::endl;
#endif

  if (is<RuleStopState*>(config->state)) {
#if DEBUG_ATN == 1
    if (_recog != nullptr) {
      std::cout << "closure at "
                << _recog->getRuleNames()[config->state->ruleIndex]
                << " rule stop " << config << std::endl;
    } else {
      std::cout << "closure at rule stop " << config << std::endl;
    }
#endif

    if (config->context == nullptr || config->context->hasEmptyPath()) {
      if (config->context == nullptr || config->context->isEmpty()) {
        configs->add(config);
        return true;
      } else {
        configs->add(std::make_shared<LexerATNConfig>(
            config, config->state, PredictionContext::EMPTY));
        currentAltReachedAcceptState = true;
      }
    }

    if (config->context != nullptr && !config->context->isEmpty()) {
      for (size_t i = 0; i < config->context->size(); i++) {
        if (config->context->getReturnState(i) !=
            PredictionContext::EMPTY_RETURN_STATE) {
          std::weak_ptr<PredictionContext> newContext =
              config->context->getParent(i);  // "pop" return state
          ATNState* returnState =
              atn.states[config->context->getReturnState(i)];
          Ref<LexerATNConfig> c = std::make_shared<LexerATNConfig>(
              config, returnState, newContext.lock());
          currentAltReachedAcceptState =
              closure(input, c, configs, currentAltReachedAcceptState,
                      speculative, treatEofAsEpsilon);
        }
      }
    }

    return currentAltReachedAcceptState;
  }

  // optimization
  if (!config->state->epsilonOnlyTransitions) {
    if (!currentAltReachedAcceptState ||
        !config->hasPassedThroughNonGreedyDecision()) {
      configs->add(config);
    }
  }

  ATNState* p = config->state;
  for (size_t i = 0; i < p->transitions.size(); i++) {
    Transition* t = p->transitions[i];
    Ref<LexerATNConfig> c = getEpsilonTarget(input, config, t, configs,
                                             speculative, treatEofAsEpsilon);
    if (c != nullptr) {
      currentAltReachedAcceptState =
          closure(input, c, configs, currentAltReachedAcceptState, speculative,
                  treatEofAsEpsilon);
    }
  }

  return currentAltReachedAcceptState;
}

Ref<LexerATNConfig> LexerATNSimulator::getEpsilonTarget(
    CharStream* input, const Ref<LexerATNConfig>& config, Transition* t,
    ATNConfigSet* configs, bool speculative, bool treatEofAsEpsilon) {
  Ref<LexerATNConfig> c = nullptr;
  switch (t->getSerializationType()) {
    case Transition::RULE: {
      RuleTransition* ruleTransition = static_cast<RuleTransition*>(t);
      Ref<PredictionContext> newContext = SingletonPredictionContext::create(
          config->context, ruleTransition->followState->stateNumber);
      c = std::make_shared<LexerATNConfig>(config, t->target, newContext);
      break;
    }

    case Transition::PRECEDENCE:
      throw UnsupportedOperationException(
          "Precedence predicates are not supported in lexers.");

    case Transition::PREDICATE: {
      /*  Track traversing semantic predicates. If we traverse,
       we cannot add a DFA state for this "reach" computation
       because the DFA would not test the predicate again in the
       future. Rather than creating collections of semantic predicates
       like v3 and testing them on prediction, v4 will test them on the
       fly all the time using the ATN not the DFA. This is slower but
       semantically it's not used that often. One of the key elements to
       this predicate mechanism is not adding DFA states that see
       predicates immediately afterwards in the ATN. For example,

       a : ID {p1}? | ID {p2}? ;

       should create the start state for rule 'a' (to save start state
       competition), but should not create target of ID state. The
       collection of ATN states the following ID references includes
       states reached by traversing predicates. Since this is when we
       test them, we cannot cash the DFA state target of ID.
       */
      PredicateTransition* pt = static_cast<PredicateTransition*>(t);

#if DEBUG_ATN == 1
      std::cout << "EVAL rule " << pt->ruleIndex << ":" << pt->predIndex
                << std::endl;
#endif

      configs->hasSemanticContext = true;
      if (evaluatePredicate(input, pt->ruleIndex, pt->predIndex, speculative)) {
        c = std::make_shared<LexerATNConfig>(config, t->target);
      }
      break;
    }

    case Transition::ACTION:
      if (config->context == nullptr || config->context->hasEmptyPath()) {
        // execute actions anywhere in the start rule for a token.
        //
        // TO_DO: if the entry rule is invoked recursively, some
        // actions may be executed during the recursive call. The
        // problem can appear when hasEmptyPath() is true but
        // isEmpty() is false. In this case, the config needs to be
        // split into two contexts - one with just the empty path
        // and another with everything but the empty path.
        // Unfortunately, the current algorithm does not allow
        // getEpsilonTarget to return two configurations, so
        // additional modifications are needed before we can support
        // the split operation.
        Ref<LexerActionExecutor> lexerActionExecutor =
            LexerActionExecutor::append(
                config->getLexerActionExecutor(),
                atn.lexerActions[static_cast<ActionTransition*>(t)
                                     ->actionIndex]);
        c = std::make_shared<LexerATNConfig>(config, t->target,
                                             lexerActionExecutor);
        break;
      } else {
        // ignore actions in referenced rules
        c = std::make_shared<LexerATNConfig>(config, t->target);
        break;
      }

    case Transition::EPSILON:
      c = std::make_shared<LexerATNConfig>(config, t->target);
      break;

    case Transition::ATOM:
    case Transition::RANGE:
    case Transition::SET:
      if (treatEofAsEpsilon) {
        if (t->matches(Token::EOF, Lexer::MIN_CHAR_VALUE,
                       Lexer::MAX_CHAR_VALUE)) {
          c = std::make_shared<LexerATNConfig>(config, t->target);
          break;
        }
      }

      break;

    default
        :  // To silence the compiler. Other transition types are not used here.
      break;
  }

  return c;
}

bool LexerATNSimulator::evaluatePredicate(CharStream* input, size_t ruleIndex,
                                          size_t predIndex, bool speculative) {
  // assume true if no recognizer was provided
  if (_recog == nullptr) {
    return true;
  }

  if (!speculative) {
    return _recog->sempred(nullptr, ruleIndex, predIndex);
  }

  size_t savedCharPositionInLine = _charPositionInLine;
  size_t savedLine = _line;
  size_t index = input->index();
  ssize_t marker = input->mark();

  auto onExit =
      finally([this, input, savedCharPositionInLine, savedLine, index, marker] {
        _charPositionInLine = savedCharPositionInLine;
        _line = savedLine;
        input->seek(index);
        input->release(marker);
      });

  consume(input);
  return _recog->sempred(nullptr, ruleIndex, predIndex);
}

void LexerATNSimulator::captureSimState(CharStream* input,
                                        dfa::DFAState* dfaState) {
  _prevAccept.index = input->index();
  _prevAccept.line = _line;
  _prevAccept.charPos = _charPositionInLine;
  _prevAccept.dfaState = dfaState;
}

dfa::DFAState* LexerATNSimulator::addDFAEdge(dfa::DFAState* from, size_t t,
                                             ATNConfigSet* q) {
  /* leading to this call, ATNConfigSet.hasSemanticContext is used as a
   * marker indicating dynamic predicate evaluation makes this edge
   * dependent on the specific input sequence, so the static edge in the
   * DFA should be omitted. The target DFAState is still created since
   * execATN has the ability to resynchronize with the DFA state cache
   * following the predicate evaluation step.
   *
   * TJP notes: next time through the DFA, we see a pred again and eval.
   * If that gets us to a previously created (but dangling) DFA
   * state, we can continue in pure DFA mode from there.
   */
  bool suppressEdge = q->hasSemanticContext;
  q->hasSemanticContext = false;

  dfa::DFAState* to = addDFAState(q);

  if (suppressEdge) {
    return to;
  }

  addDFAEdge(from, t, to);
  return to;
}

void LexerATNSimulator::addDFAEdge(dfa::DFAState* p, size_t t,
                                   dfa::DFAState* q) {
  if (/*t < MIN_DFA_EDGE ||*/ t > MAX_DFA_EDGE) {  // MIN_DFA_EDGE is 0
    // Only track edges within the DFA bounds
    return;
  }

  _edgeLock.writeLock();
  p->edges[t - MIN_DFA_EDGE] = q;  // connect
  _edgeLock.writeUnlock();
}

dfa::DFAState* LexerATNSimulator::addDFAState(ATNConfigSet* configs) {
  /* the lexer evaluates predicates on-the-fly; by this point configs
   * should not contain any configurations with unevaluated predicates.
   */
  assert(!configs->hasSemanticContext);

  dfa::DFAState* proposed = new dfa::DFAState(std::unique_ptr<ATNConfigSet>(
      configs)); /* mem-check: managed by the DFA or deleted below */
  Ref<ATNConfig> firstConfigWithRuleStopState = nullptr;
  for (auto& c : configs->configs) {
    if (is<RuleStopState*>(c->state)) {
      firstConfigWithRuleStopState = c;
      break;
    }
  }

  if (firstConfigWithRuleStopState != nullptr) {
    proposed->isAcceptState = true;
    proposed->lexerActionExecutor =
        std::dynamic_pointer_cast<LexerATNConfig>(firstConfigWithRuleStopState)
            ->getLexerActionExecutor();
    proposed->prediction =
        atn.ruleToTokenType[firstConfigWithRuleStopState->state->ruleIndex];
  }

  dfa::DFA& dfa = _decisionToDFA[_mode];

  _stateLock.writeLock();
  if (!dfa.states.empty()) {
    auto iterator = dfa.states.find(proposed);
    if (iterator != dfa.states.end()) {
      delete proposed;
      _stateLock.writeUnlock();
      return *iterator;
    }
  }

  proposed->stateNumber = (int)dfa.states.size();
  proposed->configs->setReadonly(true);

  dfa.states.insert(proposed);
  _stateLock.writeUnlock();

  return proposed;
}

dfa::DFA& LexerATNSimulator::getDFA(size_t mode) {
  return _decisionToDFA[mode];
}

std::string LexerATNSimulator::getText(CharStream* input) {
  // index is first lookahead char, don't include.
  return input->getText(misc::Interval(_startIndex, input->index() - 1));
}

size_t LexerATNSimulator::getLine() const { return _line; }

void LexerATNSimulator::setLine(size_t line) { _line = line; }

size_t LexerATNSimulator::getCharPositionInLine() {
  return _charPositionInLine;
}

void LexerATNSimulator::setCharPositionInLine(size_t charPositionInLine) {
  _charPositionInLine = charPositionInLine;
}

void LexerATNSimulator::consume(CharStream* input) {
  size_t curChar = input->LA(1);
  if (curChar == '\n') {
    _line++;
    _charPositionInLine = 0;
  } else {
    _charPositionInLine++;
  }
  input->consume();
}

std::string LexerATNSimulator::getTokenName(size_t t) {
  if (t == Token::EOF) {
    return "EOF";
  }
  return std::string("'") + static_cast<char>(t) + std::string("'");
}

void LexerATNSimulator::InitializeInstanceFields() {
  _startIndex = 0;
  _line = 1;
  _charPositionInLine = 0;
  _mode = antlr4::Lexer::DEFAULT_MODE;
}
