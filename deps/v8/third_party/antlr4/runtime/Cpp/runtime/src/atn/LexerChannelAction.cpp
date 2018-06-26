/* Copyright (c) 2012-2017 The ANTLR Project. All rights reserved.
 * Use of this file is governed by the BSD 3-clause license that
 * can be found in the LICENSE.txt file in the project root.
 */

#include "Lexer.h"
#include "misc/MurmurHash.h"

#include "atn/LexerChannelAction.h"

using namespace antlr4::atn;
using namespace antlr4::misc;

LexerChannelAction::LexerChannelAction(int channel) : _channel(channel) {}

int LexerChannelAction::getChannel() const { return _channel; }

LexerActionType LexerChannelAction::getActionType() const {
  return LexerActionType::CHANNEL;
}

bool LexerChannelAction::isPositionDependent() const { return false; }

void LexerChannelAction::execute(Lexer* lexer) { lexer->setChannel(_channel); }

size_t LexerChannelAction::hashCode() const {
  size_t hash = MurmurHash::initialize();
  hash = MurmurHash::update(hash, static_cast<size_t>(getActionType()));
  hash = MurmurHash::update(hash, _channel);
  return MurmurHash::finish(hash, 2);
}

bool LexerChannelAction::operator==(const LexerAction& obj) const {
  if (&obj == this) {
    return true;
  }

  const LexerChannelAction* action =
      dynamic_cast<const LexerChannelAction*>(&obj);
  if (action == nullptr) {
    return false;
  }

  return _channel == action->_channel;
}

std::string LexerChannelAction::toString() const {
  return "channel(" + std::to_string(_channel) + ")";
}
