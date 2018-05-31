/* Copyright (c) 2012-2017 The ANTLR Project. All rights reserved.
 * Use of this file is governed by the BSD 3-clause license that
 * can be found in the LICENSE.txt file in the project root.
 */

#include "Lexer.h"
#include "misc/MurmurHash.h"

#include "atn/LexerSkipAction.h"

using namespace antlr4;
using namespace antlr4::atn;
using namespace antlr4::misc;

const Ref<LexerSkipAction> LexerSkipAction::getInstance() {
  static Ref<LexerSkipAction> instance(new LexerSkipAction());
  return instance;
}

LexerSkipAction::LexerSkipAction() {}

LexerActionType LexerSkipAction::getActionType() const {
  return LexerActionType::SKIP;
}

bool LexerSkipAction::isPositionDependent() const { return false; }

void LexerSkipAction::execute(Lexer* lexer) { lexer->skip(); }

size_t LexerSkipAction::hashCode() const {
  size_t hash = MurmurHash::initialize();
  hash = MurmurHash::update(hash, static_cast<size_t>(getActionType()));
  return MurmurHash::finish(hash, 1);
}

bool LexerSkipAction::operator==(const LexerAction& obj) const {
  return &obj == this;
}

std::string LexerSkipAction::toString() const { return "skip"; }
