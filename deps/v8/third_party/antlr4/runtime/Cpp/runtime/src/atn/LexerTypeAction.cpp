/* Copyright (c) 2012-2017 The ANTLR Project. All rights reserved.
 * Use of this file is governed by the BSD 3-clause license that
 * can be found in the LICENSE.txt file in the project root.
 */

#include "Lexer.h"
#include "misc/MurmurHash.h"

#include "atn/LexerTypeAction.h"

using namespace antlr4;
using namespace antlr4::atn;
using namespace antlr4::misc;

LexerTypeAction::LexerTypeAction(int type) : _type(type) {}

int LexerTypeAction::getType() const { return _type; }

LexerActionType LexerTypeAction::getActionType() const {
  return LexerActionType::TYPE;
}

bool LexerTypeAction::isPositionDependent() const { return false; }

void LexerTypeAction::execute(Lexer* lexer) { lexer->setType(_type); }

size_t LexerTypeAction::hashCode() const {
  size_t hash = MurmurHash::initialize();
  hash = MurmurHash::update(hash, static_cast<size_t>(getActionType()));
  hash = MurmurHash::update(hash, _type);
  return MurmurHash::finish(hash, 2);
}

bool LexerTypeAction::operator==(const LexerAction& obj) const {
  if (&obj == this) {
    return true;
  }

  const LexerTypeAction* action = dynamic_cast<const LexerTypeAction*>(&obj);
  if (action == nullptr) {
    return false;
  }

  return _type == action->_type;
}

std::string LexerTypeAction::toString() const {
  return "type(" + std::to_string(_type) + ")";
}
