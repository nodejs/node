/* Copyright (c) 2012-2017 The ANTLR Project. All rights reserved.
 * Use of this file is governed by the BSD 3-clause license that
 * can be found in the LICENSE.txt file in the project root.
 */

#include "atn/LexerIndexedCustomAction.h"
#include "misc/MurmurHash.h"
#include "support/Arrays.h"
#include "support/CPPUtils.h"

#include "atn/LexerActionExecutor.h"

using namespace antlr4;
using namespace antlr4::atn;
using namespace antlr4::misc;
using namespace antlrcpp;

LexerActionExecutor::LexerActionExecutor(
    const std::vector<Ref<LexerAction>>& lexerActions)
    : _lexerActions(lexerActions), _hashCode(generateHashCode()) {}

LexerActionExecutor::~LexerActionExecutor() {}

Ref<LexerActionExecutor> LexerActionExecutor::append(
    Ref<LexerActionExecutor> const& lexerActionExecutor,
    Ref<LexerAction> const& lexerAction) {
  if (lexerActionExecutor == nullptr) {
    return std::make_shared<LexerActionExecutor>(
        std::vector<Ref<LexerAction>>{lexerAction});
  }

  std::vector<Ref<LexerAction>> lexerActions =
      lexerActionExecutor->_lexerActions;  // Make a copy.
  lexerActions.push_back(lexerAction);
  return std::make_shared<LexerActionExecutor>(lexerActions);
}

Ref<LexerActionExecutor> LexerActionExecutor::fixOffsetBeforeMatch(
    int offset, const Ref<LexerActionExecutor>& this_ref) {
  std::vector<Ref<LexerAction>> updatedLexerActions;
  for (size_t i = 0; i < _lexerActions.size(); i++) {
    if (_lexerActions[i]->isPositionDependent() &&
        !is<LexerIndexedCustomAction>(_lexerActions[i])) {
      if (updatedLexerActions.empty()) {
        updatedLexerActions = _lexerActions;  // Make a copy.
      }

      updatedLexerActions[i] =
          std::make_shared<LexerIndexedCustomAction>(offset, _lexerActions[i]);
    }
  }

  if (updatedLexerActions.empty()) {
    return this_ref;
  }

  return std::make_shared<LexerActionExecutor>(updatedLexerActions);
}

std::vector<Ref<LexerAction>> LexerActionExecutor::getLexerActions() const {
  return _lexerActions;
}

void LexerActionExecutor::execute(Lexer* lexer, CharStream* input,
                                  size_t startIndex) {
  bool requiresSeek = false;
  size_t stopIndex = input->index();

  auto onExit = finally([requiresSeek, input, stopIndex]() {
    if (requiresSeek) {
      input->seek(stopIndex);
    }
  });
  for (auto lexerAction : _lexerActions) {
    if (is<LexerIndexedCustomAction>(lexerAction)) {
      int offset =
          (std::static_pointer_cast<LexerIndexedCustomAction>(lexerAction))
              ->getOffset();
      input->seek(startIndex + offset);
      lexerAction =
          std::static_pointer_cast<LexerIndexedCustomAction>(lexerAction)
              ->getAction();
      requiresSeek = (startIndex + offset) != stopIndex;
    } else if (lexerAction->isPositionDependent()) {
      input->seek(stopIndex);
      requiresSeek = false;
    }

    lexerAction->execute(lexer);
  }
}

size_t LexerActionExecutor::hashCode() const { return _hashCode; }

bool LexerActionExecutor::operator==(const LexerActionExecutor& obj) const {
  if (&obj == this) {
    return true;
  }

  return _hashCode == obj._hashCode &&
         Arrays::equals(_lexerActions, obj._lexerActions);
}

bool LexerActionExecutor::operator!=(const LexerActionExecutor& obj) const {
  return !operator==(obj);
}

size_t LexerActionExecutor::generateHashCode() const {
  size_t hash = MurmurHash::initialize();
  for (auto lexerAction : _lexerActions) {
    hash = MurmurHash::update(hash, lexerAction);
  }
  hash = MurmurHash::finish(hash, _lexerActions.size());

  return hash;
}
