/* Copyright (c) 2012-2017 The ANTLR Project. All rights reserved.
 * Use of this file is governed by the BSD 3-clause license that
 * can be found in the LICENSE.txt file in the project root.
 */

#include "Lexer.h"
#include "misc/MurmurHash.h"

#include "atn/LexerMoreAction.h"

using namespace antlr4;
using namespace antlr4::atn;
using namespace antlr4::misc;

const Ref<LexerMoreAction> LexerMoreAction::getInstance() {
  static Ref<LexerMoreAction> instance(new LexerMoreAction());
  return instance;
}

LexerMoreAction::LexerMoreAction() {}

LexerActionType LexerMoreAction::getActionType() const {
  return LexerActionType::MORE;
}

bool LexerMoreAction::isPositionDependent() const { return false; }

void LexerMoreAction::execute(Lexer* lexer) { lexer->more(); }

size_t LexerMoreAction::hashCode() const {
  size_t hash = MurmurHash::initialize();
  hash = MurmurHash::update(hash, static_cast<size_t>(getActionType()));
  return MurmurHash::finish(hash, 1);
}

bool LexerMoreAction::operator==(const LexerAction& obj) const {
  return &obj == this;
}

std::string LexerMoreAction::toString() const { return "more"; }
