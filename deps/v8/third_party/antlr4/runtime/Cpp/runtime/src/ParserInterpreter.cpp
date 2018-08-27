/* Copyright (c) 2012-2017 The ANTLR Project. All rights reserved.
 * Use of this file is governed by the BSD 3-clause license that
 * can be found in the LICENSE.txt file in the project root.
 */

#include "ANTLRErrorStrategy.h"
#include "CommonToken.h"
#include "FailedPredicateException.h"
#include "InputMismatchException.h"
#include "InterpreterRuleContext.h"
#include "Lexer.h"
#include "Token.h"
#include "Vocabulary.h"
#include "atn/ATN.h"
#include "atn/ActionTransition.h"
#include "atn/AtomTransition.h"
#include "atn/LoopEndState.h"
#include "atn/ParserATNSimulator.h"
#include "atn/PrecedencePredicateTransition.h"
#include "atn/PredicateTransition.h"
#include "atn/RuleStartState.h"
#include "atn/RuleStopState.h"
#include "atn/RuleTransition.h"
#include "atn/StarLoopEntryState.h"
#include "dfa/DFA.h"
#include "tree/ErrorNode.h"

#include "support/CPPUtils.h"

#include "ParserInterpreter.h"

using namespace antlr4;
using namespace antlr4::atn;
using namespace antlr4::misc;

using namespace antlrcpp;

ParserInterpreter::ParserInterpreter(const std::string& grammarFileName,
                                     const std::vector<std::string>& tokenNames,
                                     const std::vector<std::string>& ruleNames,
                                     const atn::ATN& atn, TokenStream* input)
    : ParserInterpreter(grammarFileName,
                        dfa::Vocabulary::fromTokenNames(tokenNames), ruleNames,
                        atn, input) {}

ParserInterpreter::ParserInterpreter(const std::string& grammarFileName,
                                     const dfa::Vocabulary& vocabulary,
                                     const std::vector<std::string>& ruleNames,
                                     const atn::ATN& atn, TokenStream* input)
    : Parser(input),
      _grammarFileName(grammarFileName),
      _atn(atn),
      _ruleNames(ruleNames),
      _vocabulary(vocabulary) {
  for (size_t i = 0; i < atn.maxTokenType; ++i) {
    _tokenNames.push_back(vocabulary.getDisplayName(i));
  }

  // init decision DFA
  for (size_t i = 0; i < atn.getNumberOfDecisions(); ++i) {
    atn::DecisionState* decisionState = atn.getDecisionState(i);
    _decisionToDFA.push_back(dfa::DFA(decisionState, i));
  }

  // get atn simulator that knows how to do predictions
  _interpreter = new atn::ParserATNSimulator(
      this, atn, _decisionToDFA,
      _sharedContextCache); /* mem-check: deleted in d-tor */
}

ParserInterpreter::~ParserInterpreter() { delete _interpreter; }

void ParserInterpreter::reset() {
  Parser::reset();
  _overrideDecisionReached = false;
  _overrideDecisionRoot = nullptr;
}

const atn::ATN& ParserInterpreter::getATN() const { return _atn; }

const std::vector<std::string>& ParserInterpreter::getTokenNames() const {
  return _tokenNames;
}

const dfa::Vocabulary& ParserInterpreter::getVocabulary() const {
  return _vocabulary;
}

const std::vector<std::string>& ParserInterpreter::getRuleNames() const {
  return _ruleNames;
}

std::string ParserInterpreter::getGrammarFileName() const {
  return _grammarFileName;
}

ParserRuleContext* ParserInterpreter::parse(size_t startRuleIndex) {
  atn::RuleStartState* startRuleStartState =
      _atn.ruleToStartState[startRuleIndex];

  _rootContext = createInterpreterRuleContext(
      nullptr, atn::ATNState::INVALID_STATE_NUMBER, startRuleIndex);

  if (startRuleStartState->isLeftRecursiveRule) {
    enterRecursionRule(_rootContext, startRuleStartState->stateNumber,
                       startRuleIndex, 0);
  } else {
    enterRule(_rootContext, startRuleStartState->stateNumber, startRuleIndex);
  }

  while (true) {
    atn::ATNState* p = getATNState();
    switch (p->getStateType()) {
      case atn::ATNState::RULE_STOP:
        // pop; return from rule
        if (_ctx->isEmpty()) {
          if (startRuleStartState->isLeftRecursiveRule) {
            ParserRuleContext* result = _ctx;
            auto parentContext = _parentContextStack.top();
            _parentContextStack.pop();
            unrollRecursionContexts(parentContext.first);
            return result;
          } else {
            exitRule();
            return _rootContext;
          }
        }

        visitRuleStopState(p);
        break;

      default:
        try {
          visitState(p);
        } catch (RecognitionException& e) {
          setState(_atn.ruleToStopState[p->ruleIndex]->stateNumber);
          getErrorHandler()->reportError(this, e);
          getContext()->exception = std::current_exception();
          recover(e);
        }

        break;
    }
  }
}

void ParserInterpreter::enterRecursionRule(ParserRuleContext* localctx,
                                           size_t state, size_t ruleIndex,
                                           int precedence) {
  _parentContextStack.push({_ctx, localctx->invokingState});
  Parser::enterRecursionRule(localctx, state, ruleIndex, precedence);
}

void ParserInterpreter::addDecisionOverride(int decision, int tokenIndex,
                                            int forcedAlt) {
  _overrideDecision = decision;
  _overrideDecisionInputIndex = tokenIndex;
  _overrideDecisionAlt = forcedAlt;
}

Ref<InterpreterRuleContext> ParserInterpreter::getOverrideDecisionRoot() const {
  return _overrideDecisionRoot;
}

InterpreterRuleContext* ParserInterpreter::getRootContext() {
  return _rootContext;
}

atn::ATNState* ParserInterpreter::getATNState() {
  return _atn.states[getState()];
}

void ParserInterpreter::visitState(atn::ATNState* p) {
  size_t predictedAlt = 1;
  if (is<DecisionState*>(p)) {
    predictedAlt = visitDecisionState(dynamic_cast<DecisionState*>(p));
  }

  atn::Transition* transition = p->transitions[predictedAlt - 1];
  switch (transition->getSerializationType()) {
    case atn::Transition::EPSILON:
      if (p->getStateType() == ATNState::STAR_LOOP_ENTRY &&
          (dynamic_cast<StarLoopEntryState*>(p))->isPrecedenceDecision &&
          !is<LoopEndState*>(transition->target)) {
        // We are at the start of a left recursive rule's (...)* loop
        // and we're not taking the exit branch of loop.
        InterpreterRuleContext* localctx = createInterpreterRuleContext(
            _parentContextStack.top().first, _parentContextStack.top().second,
            static_cast<int>(_ctx->getRuleIndex()));
        pushNewRecursionContext(
            localctx, _atn.ruleToStartState[p->ruleIndex]->stateNumber,
            static_cast<int>(_ctx->getRuleIndex()));
      }
      break;

    case atn::Transition::ATOM:
      match(static_cast<int>(
          static_cast<atn::AtomTransition*>(transition)->_label));
      break;

    case atn::Transition::RANGE:
    case atn::Transition::SET:
    case atn::Transition::NOT_SET:
      if (!transition->matches(static_cast<int>(_input->LA(1)),
                               Token::MIN_USER_TOKEN_TYPE,
                               Lexer::MAX_CHAR_VALUE)) {
        recoverInline();
      }
      matchWildcard();
      break;

    case atn::Transition::WILDCARD:
      matchWildcard();
      break;

    case atn::Transition::RULE: {
      atn::RuleStartState* ruleStartState =
          static_cast<atn::RuleStartState*>(transition->target);
      size_t ruleIndex = ruleStartState->ruleIndex;
      InterpreterRuleContext* newctx =
          createInterpreterRuleContext(_ctx, p->stateNumber, ruleIndex);
      if (ruleStartState->isLeftRecursiveRule) {
        enterRecursionRule(
            newctx, ruleStartState->stateNumber, ruleIndex,
            static_cast<atn::RuleTransition*>(transition)->precedence);
      } else {
        enterRule(newctx, transition->target->stateNumber, ruleIndex);
      }
    } break;

    case atn::Transition::PREDICATE: {
      atn::PredicateTransition* predicateTransition =
          static_cast<atn::PredicateTransition*>(transition);
      if (!sempred(_ctx, predicateTransition->ruleIndex,
                   predicateTransition->predIndex)) {
        throw FailedPredicateException(this);
      }
    } break;

    case atn::Transition::ACTION: {
      atn::ActionTransition* actionTransition =
          static_cast<atn::ActionTransition*>(transition);
      action(_ctx, actionTransition->ruleIndex, actionTransition->actionIndex);
    } break;

    case atn::Transition::PRECEDENCE: {
      if (!precpred(_ctx,
                    static_cast<atn::PrecedencePredicateTransition*>(transition)
                        ->precedence)) {
        throw FailedPredicateException(
            this,
            "precpred(_ctx, " +
                std::to_string(
                    static_cast<atn::PrecedencePredicateTransition*>(transition)
                        ->precedence) +
                ")");
      }
    } break;

    default:
      throw UnsupportedOperationException("Unrecognized ATN transition type.");
  }

  setState(transition->target->stateNumber);
}

size_t ParserInterpreter::visitDecisionState(DecisionState* p) {
  size_t predictedAlt = 1;
  if (p->transitions.size() > 1) {
    getErrorHandler()->sync(this);
    int decision = p->decision;
    if (decision == _overrideDecision &&
        _input->index() == _overrideDecisionInputIndex &&
        !_overrideDecisionReached) {
      predictedAlt = _overrideDecisionAlt;
      _overrideDecisionReached = true;
    } else {
      predictedAlt = getInterpreter<ParserATNSimulator>()->adaptivePredict(
          _input, decision, _ctx);
    }
  }
  return predictedAlt;
}

InterpreterRuleContext* ParserInterpreter::createInterpreterRuleContext(
    ParserRuleContext* parent, size_t invokingStateNumber, size_t ruleIndex) {
  return _tracker.createInstance<InterpreterRuleContext>(
      parent, invokingStateNumber, ruleIndex);
}

void ParserInterpreter::visitRuleStopState(atn::ATNState* p) {
  atn::RuleStartState* ruleStartState = _atn.ruleToStartState[p->ruleIndex];
  if (ruleStartState->isLeftRecursiveRule) {
    std::pair<ParserRuleContext*, size_t> parentContext =
        _parentContextStack.top();
    _parentContextStack.pop();

    unrollRecursionContexts(parentContext.first);
    setState(parentContext.second);
  } else {
    exitRule();
  }

  atn::RuleTransition* ruleTransition = static_cast<atn::RuleTransition*>(
      _atn.states[getState()]->transitions[0]);
  setState(ruleTransition->followState->stateNumber);
}

void ParserInterpreter::recover(RecognitionException& e) {
  size_t i = _input->index();
  getErrorHandler()->recover(this, std::make_exception_ptr(e));

  if (_input->index() == i) {
    // no input consumed, better add an error node
    if (is<InputMismatchException*>(&e)) {
      InputMismatchException& ime = static_cast<InputMismatchException&>(e);
      Token* tok = e.getOffendingToken();
      size_t expectedTokenType =
          ime.getExpectedTokens().getMinElement();  // get any element
      _errorToken = getTokenFactory()->create(
          {tok->getTokenSource(), tok->getTokenSource()->getInputStream()},
          expectedTokenType, tok->getText(), Token::DEFAULT_CHANNEL,
          INVALID_INDEX, INVALID_INDEX,  // invalid start/stop
          tok->getLine(), tok->getCharPositionInLine());
      _ctx->addChild(createErrorNode(_errorToken.get()));
    } else {  // NoViableAlt
      Token* tok = e.getOffendingToken();
      _errorToken = getTokenFactory()->create(
          {tok->getTokenSource(), tok->getTokenSource()->getInputStream()},
          Token::INVALID_TYPE, tok->getText(), Token::DEFAULT_CHANNEL,
          INVALID_INDEX, INVALID_INDEX,  // invalid start/stop
          tok->getLine(), tok->getCharPositionInLine());
      _ctx->addChild(createErrorNode(_errorToken.get()));
    }
  }
}

Token* ParserInterpreter::recoverInline() {
  return _errHandler->recoverInline(this);
}
