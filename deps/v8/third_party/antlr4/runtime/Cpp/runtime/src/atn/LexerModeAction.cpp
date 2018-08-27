/* Copyright (c) 2012-2017 The ANTLR Project. All rights reserved.
 * Use of this file is governed by the BSD 3-clause license that
 * can be found in the LICENSE.txt file in the project root.
 */

#include "Lexer.h"
#include "misc/MurmurHash.h"

#include "atn/LexerModeAction.h"

using namespace antlr4;
using namespace antlr4::atn;
using namespace antlr4::misc;

LexerModeAction::LexerModeAction(int mode) : _mode(mode) {}

int LexerModeAction::getMode() { return _mode; }

LexerActionType LexerModeAction::getActionType() const {
  return LexerActionType::MODE;
}

bool LexerModeAction::isPositionDependent() const { return false; }

void LexerModeAction::execute(Lexer* lexer) { lexer->setMode(_mode); }

size_t LexerModeAction::hashCode() const {
  size_t hash = MurmurHash::initialize();
  hash = MurmurHash::update(hash, static_cast<size_t>(getActionType()));
  hash = MurmurHash::update(hash, _mode);
  return MurmurHash::finish(hash, 2);
}

bool LexerModeAction::operator==(const LexerAction& obj) const {
  if (&obj == this) {
    return true;
  }

  const LexerModeAction* action = dynamic_cast<const LexerModeAction*>(&obj);
  if (action == nullptr) {
    return false;
  }

  return _mode == action->_mode;
}

std::string LexerModeAction::toString() const {
  return "mode(" + std::to_string(_mode) + ")";
}
