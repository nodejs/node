/* Copyright (c) 2012-2017 The ANTLR Project. All rights reserved.
 * Use of this file is governed by the BSD 3-clause license that
 * can be found in the LICENSE.txt file in the project root.
 */

#include "Lexer.h"
#include "misc/MurmurHash.h"

#include "atn/LexerPushModeAction.h"

using namespace antlr4;
using namespace antlr4::atn;
using namespace antlr4::misc;

LexerPushModeAction::LexerPushModeAction(int mode) : _mode(mode) {}

int LexerPushModeAction::getMode() const { return _mode; }

LexerActionType LexerPushModeAction::getActionType() const {
  return LexerActionType::PUSH_MODE;
}

bool LexerPushModeAction::isPositionDependent() const { return false; }

void LexerPushModeAction::execute(Lexer* lexer) { lexer->pushMode(_mode); }

size_t LexerPushModeAction::hashCode() const {
  size_t hash = MurmurHash::initialize();
  hash = MurmurHash::update(hash, static_cast<size_t>(getActionType()));
  hash = MurmurHash::update(hash, _mode);
  return MurmurHash::finish(hash, 2);
}

bool LexerPushModeAction::operator==(const LexerAction& obj) const {
  if (&obj == this) {
    return true;
  }

  const LexerPushModeAction* action =
      dynamic_cast<const LexerPushModeAction*>(&obj);
  if (action == nullptr) {
    return false;
  }

  return _mode == action->_mode;
}

std::string LexerPushModeAction::toString() const {
  return "pushMode(" + std::to_string(_mode) + ")";
}
