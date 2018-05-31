/* Copyright (c) 2012-2017 The ANTLR Project. All rights reserved.
 * Use of this file is governed by the BSD 3-clause license that
 * can be found in the LICENSE.txt file in the project root.
 */

#include "Lexer.h"
#include "misc/MurmurHash.h"

#include "atn/LexerPopModeAction.h"

using namespace antlr4;
using namespace antlr4::atn;
using namespace antlr4::misc;

const Ref<LexerPopModeAction> LexerPopModeAction::getInstance() {
  static Ref<LexerPopModeAction> instance(new LexerPopModeAction());
  return instance;
}

LexerPopModeAction::LexerPopModeAction() {}

LexerActionType LexerPopModeAction::getActionType() const {
  return LexerActionType::POP_MODE;
}

bool LexerPopModeAction::isPositionDependent() const { return false; }

void LexerPopModeAction::execute(Lexer* lexer) { lexer->popMode(); }

size_t LexerPopModeAction::hashCode() const {
  size_t hash = MurmurHash::initialize();
  hash = MurmurHash::update(hash, static_cast<size_t>(getActionType()));
  return MurmurHash::finish(hash, 1);
}

bool LexerPopModeAction::operator==(const LexerAction& obj) const {
  return &obj == this;
}

std::string LexerPopModeAction::toString() const { return "popMode"; }
