/* Copyright (c) 2012-2017 The ANTLR Project. All rights reserved.
 * Use of this file is governed by the BSD 3-clause license that
 * can be found in the LICENSE.txt file in the project root.
 */

#include "Token.h"

#include "CommonTokenStream.h"

using namespace antlr4;

CommonTokenStream::CommonTokenStream(TokenSource* tokenSource)
    : CommonTokenStream(tokenSource, Token::DEFAULT_CHANNEL) {}

CommonTokenStream::CommonTokenStream(TokenSource* tokenSource, size_t channel_)
    : BufferedTokenStream(tokenSource), channel(channel_) {}

ssize_t CommonTokenStream::adjustSeekIndex(size_t i) {
  return nextTokenOnChannel(i, channel);
}

Token* CommonTokenStream::LB(size_t k) {
  if (k == 0 || k > _p) {
    return nullptr;
  }

  ssize_t i = static_cast<ssize_t>(_p);
  size_t n = 1;
  // find k good tokens looking backwards
  while (n <= k) {
    // skip off-channel tokens
    i = previousTokenOnChannel(i - 1, channel);
    n++;
  }
  if (i < 0) {
    return nullptr;
  }

  return _tokens[i].get();
}

Token* CommonTokenStream::LT(ssize_t k) {
  lazyInit();
  if (k == 0) {
    return nullptr;
  }
  if (k < 0) {
    return LB(static_cast<size_t>(-k));
  }
  size_t i = _p;
  ssize_t n = 1;  // we know tokens[p] is a good one
                  // find k good tokens
  while (n < k) {
    // skip off-channel tokens, but make sure to not look past EOF
    if (sync(i + 1)) {
      i = nextTokenOnChannel(i + 1, channel);
    }
    n++;
  }

  return _tokens[i].get();
}

int CommonTokenStream::getNumberOfOnChannelTokens() {
  int n = 0;
  fill();
  for (size_t i = 0; i < _tokens.size(); i++) {
    Token* t = _tokens[i].get();
    if (t->getChannel() == channel) {
      n++;
    }
    if (t->getType() == Token::EOF) {
      break;
    }
  }
  return n;
}
