/* Copyright (c) 2012-2017 The ANTLR Project. All rights reserved.
 * Use of this file is governed by the BSD 3-clause license that
 * can be found in the LICENSE.txt file in the project root.
 */

#include "Lexer.h"
#include "misc/MurmurHash.h"
#include "support/CPPUtils.h"

#include "atn/LexerIndexedCustomAction.h"

using namespace antlr4;
using namespace antlr4::atn;
using namespace antlr4::misc;

LexerIndexedCustomAction::LexerIndexedCustomAction(
    int offset, Ref<LexerAction> const& action)
    : _offset(offset), _action(action) {}

int LexerIndexedCustomAction::getOffset() const { return _offset; }

Ref<LexerAction> LexerIndexedCustomAction::getAction() const { return _action; }

LexerActionType LexerIndexedCustomAction::getActionType() const {
  return _action->getActionType();
}

bool LexerIndexedCustomAction::isPositionDependent() const { return true; }

void LexerIndexedCustomAction::execute(Lexer* lexer) {
  // assume the input stream position was properly set by the calling code
  _action->execute(lexer);
}

size_t LexerIndexedCustomAction::hashCode() const {
  size_t hash = MurmurHash::initialize();
  hash = MurmurHash::update(hash, _offset);
  hash = MurmurHash::update(hash, _action);
  return MurmurHash::finish(hash, 2);
}

bool LexerIndexedCustomAction::operator==(const LexerAction& obj) const {
  if (&obj == this) {
    return true;
  }

  const LexerIndexedCustomAction* action =
      dynamic_cast<const LexerIndexedCustomAction*>(&obj);
  if (action == nullptr) {
    return false;
  }

  return _offset == action->_offset && *_action == *action->_action;
}

std::string LexerIndexedCustomAction::toString() const {
  return antlrcpp::toString(this);
}
