/* Copyright (c) 2012-2017 The ANTLR Project. All rights reserved.
 * Use of this file is governed by the BSD 3-clause license that
 * can be found in the LICENSE.txt file in the project root.
 */

#include "SemanticContext.h"
#include "atn/DecisionState.h"
#include "atn/LexerActionExecutor.h"
#include "atn/PredictionContext.h"
#include "misc/MurmurHash.h"

#include "support/CPPUtils.h"

#include "atn/LexerATNConfig.h"

using namespace antlr4::atn;
using namespace antlrcpp;

LexerATNConfig::LexerATNConfig(ATNState* state, int alt,
                               Ref<PredictionContext> const& context)
    : ATNConfig(state, alt, context, SemanticContext::NONE),
      _passedThroughNonGreedyDecision(false) {}

LexerATNConfig::LexerATNConfig(
    ATNState* state, int alt, Ref<PredictionContext> const& context,
    Ref<LexerActionExecutor> const& lexerActionExecutor)
    : ATNConfig(state, alt, context, SemanticContext::NONE),
      _lexerActionExecutor(lexerActionExecutor),
      _passedThroughNonGreedyDecision(false) {}

LexerATNConfig::LexerATNConfig(Ref<LexerATNConfig> const& c, ATNState* state)
    : ATNConfig(c, state, c->context, c->semanticContext),
      _lexerActionExecutor(c->_lexerActionExecutor),
      _passedThroughNonGreedyDecision(checkNonGreedyDecision(c, state)) {}

LexerATNConfig::LexerATNConfig(
    Ref<LexerATNConfig> const& c, ATNState* state,
    Ref<LexerActionExecutor> const& lexerActionExecutor)
    : ATNConfig(c, state, c->context, c->semanticContext),
      _lexerActionExecutor(lexerActionExecutor),
      _passedThroughNonGreedyDecision(checkNonGreedyDecision(c, state)) {}

LexerATNConfig::LexerATNConfig(Ref<LexerATNConfig> const& c, ATNState* state,
                               Ref<PredictionContext> const& context)
    : ATNConfig(c, state, context, c->semanticContext),
      _lexerActionExecutor(c->_lexerActionExecutor),
      _passedThroughNonGreedyDecision(checkNonGreedyDecision(c, state)) {}

Ref<LexerActionExecutor> LexerATNConfig::getLexerActionExecutor() const {
  return _lexerActionExecutor;
}

bool LexerATNConfig::hasPassedThroughNonGreedyDecision() {
  return _passedThroughNonGreedyDecision;
}

size_t LexerATNConfig::hashCode() const {
  size_t hashCode = misc::MurmurHash::initialize(7);
  hashCode = misc::MurmurHash::update(hashCode, state->stateNumber);
  hashCode = misc::MurmurHash::update(hashCode, alt);
  hashCode = misc::MurmurHash::update(hashCode, context);
  hashCode = misc::MurmurHash::update(hashCode, semanticContext);
  hashCode = misc::MurmurHash::update(hashCode,
                                      _passedThroughNonGreedyDecision ? 1 : 0);
  hashCode = misc::MurmurHash::update(hashCode, _lexerActionExecutor);
  hashCode = misc::MurmurHash::finish(hashCode, 6);
  return hashCode;
}

bool LexerATNConfig::operator==(const LexerATNConfig& other) const {
  if (this == &other) return true;

  if (_passedThroughNonGreedyDecision != other._passedThroughNonGreedyDecision)
    return false;

  if (_lexerActionExecutor == nullptr)
    return other._lexerActionExecutor == nullptr;
  if (*_lexerActionExecutor != *(other._lexerActionExecutor)) {
    return false;
  }

  return ATNConfig::operator==(other);
}

bool LexerATNConfig::checkNonGreedyDecision(Ref<LexerATNConfig> const& source,
                                            ATNState* target) {
  return source->_passedThroughNonGreedyDecision ||
         (is<DecisionState*>(target) &&
          (static_cast<DecisionState*>(target))->nonGreedy);
}
