/* Copyright (c) 2012-2017 The ANTLR Project. All rights reserved.
 * Use of this file is governed by the BSD 3-clause license that
 * can be found in the LICENSE.txt file in the project root.
 */

#include "Lexer.h"
#include "misc/MurmurHash.h"
#include "support/CPPUtils.h"

#include "atn/LexerCustomAction.h"

using namespace antlr4;
using namespace antlr4::atn;
using namespace antlr4::misc;

LexerCustomAction::LexerCustomAction(size_t ruleIndex, size_t actionIndex)
    : _ruleIndex(ruleIndex), _actionIndex(actionIndex) {}

size_t LexerCustomAction::getRuleIndex() const { return _ruleIndex; }

size_t LexerCustomAction::getActionIndex() const { return _actionIndex; }

LexerActionType LexerCustomAction::getActionType() const {
  return LexerActionType::CUSTOM;
}

bool LexerCustomAction::isPositionDependent() const { return true; }

void LexerCustomAction::execute(Lexer* lexer) {
  lexer->action(nullptr, _ruleIndex, _actionIndex);
}

size_t LexerCustomAction::hashCode() const {
  size_t hash = MurmurHash::initialize();
  hash = MurmurHash::update(hash, static_cast<size_t>(getActionType()));
  hash = MurmurHash::update(hash, _ruleIndex);
  hash = MurmurHash::update(hash, _actionIndex);
  return MurmurHash::finish(hash, 3);
}

bool LexerCustomAction::operator==(const LexerAction& obj) const {
  if (&obj == this) {
    return true;
  }

  const LexerCustomAction* action =
      dynamic_cast<const LexerCustomAction*>(&obj);
  if (action == nullptr) {
    return false;
  }

  return _ruleIndex == action->_ruleIndex &&
         _actionIndex == action->_actionIndex;
}

std::string LexerCustomAction::toString() const {
  return antlrcpp::toString(this);
}
