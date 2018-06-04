/* Copyright (c) 2012-2017 The ANTLR Project. All rights reserved.
 * Use of this file is governed by the BSD 3-clause license that
 * can be found in the LICENSE.txt file in the project root.
 */

#include "Exceptions.h"
#include "Lexer.h"
#include "RuleContext.h"
#include "WritableToken.h"
#include "misc/Interval.h"
#include "support/CPPUtils.h"

#include "BufferedTokenStream.h"

using namespace antlr4;
using namespace antlrcpp;

BufferedTokenStream::BufferedTokenStream(TokenSource* tokenSource)
    : _tokenSource(tokenSource) {
  InitializeInstanceFields();
}

TokenSource* BufferedTokenStream::getTokenSource() const {
  return _tokenSource;
}

size_t BufferedTokenStream::index() { return _p; }

ssize_t BufferedTokenStream::mark() { return 0; }

void BufferedTokenStream::release(ssize_t /*marker*/) {
  // no resources to release
}

void BufferedTokenStream::reset() { seek(0); }

void BufferedTokenStream::seek(size_t index) {
  lazyInit();
  _p = adjustSeekIndex(index);
}

size_t BufferedTokenStream::size() { return _tokens.size(); }

void BufferedTokenStream::consume() {
  bool skipEofCheck = false;
  if (!_needSetup) {
    if (_fetchedEOF) {
      // the last token in tokens is EOF. skip check if p indexes any
      // fetched token except the last.
      skipEofCheck = _p < _tokens.size() - 1;
    } else {
      // no EOF token in tokens. skip check if p indexes a fetched token.
      skipEofCheck = _p < _tokens.size();
    }
  } else {
    // not yet initialized
    skipEofCheck = false;
  }

  if (!skipEofCheck && LA(1) == Token::EOF) {
    throw IllegalStateException("cannot consume EOF");
  }

  if (sync(_p + 1)) {
    _p = adjustSeekIndex(_p + 1);
  }
}

bool BufferedTokenStream::sync(size_t i) {
  if (i + 1 < _tokens.size()) return true;
  size_t n = i - _tokens.size() + 1;  // how many more elements we need?

  if (n > 0) {
    size_t fetched = fetch(n);
    return fetched >= n;
  }

  return true;
}

size_t BufferedTokenStream::fetch(size_t n) {
  if (_fetchedEOF) {
    return 0;
  }

  size_t i = 0;
  while (i < n) {
    std::unique_ptr<Token> t(_tokenSource->nextToken());

    if (is<WritableToken*>(t.get())) {
      (static_cast<WritableToken*>(t.get()))->setTokenIndex(_tokens.size());
    }

    _tokens.push_back(std::move(t));
    ++i;

    if (_tokens.back()->getType() == Token::EOF) {
      _fetchedEOF = true;
      break;
    }
  }

  return i;
}

Token* BufferedTokenStream::get(size_t i) const {
  if (i >= _tokens.size()) {
    throw IndexOutOfBoundsException(
        std::string("token index ") + std::to_string(i) +
        std::string(" out of range 0..") + std::to_string(_tokens.size() - 1));
  }
  return _tokens[i].get();
}

std::vector<Token*> BufferedTokenStream::get(size_t start, size_t stop) {
  std::vector<Token*> subset;

  lazyInit();

  if (_tokens.empty()) {
    return subset;
  }

  if (stop >= _tokens.size()) {
    stop = _tokens.size() - 1;
  }
  for (size_t i = start; i <= stop; i++) {
    Token* t = _tokens[i].get();
    if (t->getType() == Token::EOF) {
      break;
    }
    subset.push_back(t);
  }
  return subset;
}

size_t BufferedTokenStream::LA(ssize_t i) { return LT(i)->getType(); }

Token* BufferedTokenStream::LB(size_t k) {
  if (k > _p) {
    return nullptr;
  }
  return _tokens[_p - k].get();
}

Token* BufferedTokenStream::LT(ssize_t k) {
  lazyInit();
  if (k == 0) {
    return nullptr;
  }
  if (k < 0) {
    return LB(-k);
  }

  size_t i = _p + k - 1;
  sync(i);
  if (i >= _tokens.size()) {  // return EOF token
                              // EOF must be last token
    return _tokens.back().get();
  }

  return _tokens[i].get();
}

ssize_t BufferedTokenStream::adjustSeekIndex(size_t i) { return i; }

void BufferedTokenStream::lazyInit() {
  if (_needSetup) {
    setup();
  }
}

void BufferedTokenStream::setup() {
  _needSetup = false;
  sync(0);
  _p = adjustSeekIndex(0);
}

void BufferedTokenStream::setTokenSource(TokenSource* tokenSource) {
  _tokenSource = tokenSource;
  _tokens.clear();
  _fetchedEOF = false;
  _needSetup = true;
}

std::vector<Token*> BufferedTokenStream::getTokens() {
  std::vector<Token*> result;
  for (auto& t : _tokens) result.push_back(t.get());
  return result;
}

std::vector<Token*> BufferedTokenStream::getTokens(size_t start, size_t stop) {
  return getTokens(start, stop, std::vector<size_t>());
}

std::vector<Token*> BufferedTokenStream::getTokens(
    size_t start, size_t stop, const std::vector<size_t>& types) {
  lazyInit();
  if (stop >= _tokens.size() || start >= _tokens.size()) {
    throw IndexOutOfBoundsException(
        std::string("start ") + std::to_string(start) +
        std::string(" or stop ") + std::to_string(stop) +
        std::string(" not in 0..") + std::to_string(_tokens.size() - 1));
  }

  std::vector<Token*> filteredTokens;

  if (start > stop) {
    return filteredTokens;
  }

  for (size_t i = start; i <= stop; i++) {
    Token* tok = _tokens[i].get();

    if (types.empty() ||
        std::find(types.begin(), types.end(), tok->getType()) != types.end()) {
      filteredTokens.push_back(tok);
    }
  }
  return filteredTokens;
}

std::vector<Token*> BufferedTokenStream::getTokens(size_t start, size_t stop,
                                                   size_t ttype) {
  std::vector<size_t> s;
  s.push_back(ttype);
  return getTokens(start, stop, s);
}

ssize_t BufferedTokenStream::nextTokenOnChannel(size_t i, size_t channel) {
  sync(i);
  if (i >= size()) {
    return size() - 1;
  }

  Token* token = _tokens[i].get();
  while (token->getChannel() != channel) {
    if (token->getType() == Token::EOF) {
      return i;
    }
    i++;
    sync(i);
    token = _tokens[i].get();
  }
  return i;
}

ssize_t BufferedTokenStream::previousTokenOnChannel(size_t i, size_t channel) {
  sync(i);
  if (i >= size()) {
    // the EOF token is on every channel
    return size() - 1;
  }

  while (true) {
    Token* token = _tokens[i].get();
    if (token->getType() == Token::EOF || token->getChannel() == channel) {
      return i;
    }

    if (i == 0) break;
    i--;
  }
  return i;
}

std::vector<Token*> BufferedTokenStream::getHiddenTokensToRight(
    size_t tokenIndex, ssize_t channel) {
  lazyInit();
  if (tokenIndex >= _tokens.size()) {
    throw IndexOutOfBoundsException(std::to_string(tokenIndex) + " not in 0.." +
                                    std::to_string(_tokens.size() - 1));
  }

  ssize_t nextOnChannel =
      nextTokenOnChannel(tokenIndex + 1, Lexer::DEFAULT_TOKEN_CHANNEL);
  size_t to;
  size_t from = tokenIndex + 1;
  // if none onchannel to right, nextOnChannel=-1 so set to = last token
  if (nextOnChannel == -1) {
    to = static_cast<ssize_t>(size() - 1);
  } else {
    to = nextOnChannel;
  }

  return filterForChannel(from, to, channel);
}

std::vector<Token*> BufferedTokenStream::getHiddenTokensToRight(
    size_t tokenIndex) {
  return getHiddenTokensToRight(tokenIndex, -1);
}

std::vector<Token*> BufferedTokenStream::getHiddenTokensToLeft(
    size_t tokenIndex, ssize_t channel) {
  lazyInit();
  if (tokenIndex >= _tokens.size()) {
    throw IndexOutOfBoundsException(std::to_string(tokenIndex) + " not in 0.." +
                                    std::to_string(_tokens.size() - 1));
  }

  if (tokenIndex == 0) {
    // Obviously no tokens can appear before the first token.
    return {};
  }

  ssize_t prevOnChannel =
      previousTokenOnChannel(tokenIndex - 1, Lexer::DEFAULT_TOKEN_CHANNEL);
  if (prevOnChannel == static_cast<ssize_t>(tokenIndex - 1)) {
    return {};
  }
  // if none onchannel to left, prevOnChannel=-1 then from=0
  size_t from = static_cast<size_t>(prevOnChannel + 1);
  size_t to = tokenIndex - 1;

  return filterForChannel(from, to, channel);
}

std::vector<Token*> BufferedTokenStream::getHiddenTokensToLeft(
    size_t tokenIndex) {
  return getHiddenTokensToLeft(tokenIndex, -1);
}

std::vector<Token*> BufferedTokenStream::filterForChannel(size_t from,
                                                          size_t to,
                                                          ssize_t channel) {
  std::vector<Token*> hidden;
  for (size_t i = from; i <= to; i++) {
    Token* t = _tokens[i].get();
    if (channel == -1) {
      if (t->getChannel() != Lexer::DEFAULT_TOKEN_CHANNEL) {
        hidden.push_back(t);
      }
    } else {
      if (t->getChannel() == static_cast<size_t>(channel)) {
        hidden.push_back(t);
      }
    }
  }

  return hidden;
}

bool BufferedTokenStream::isInitialized() const { return !_needSetup; }

/**
 * Get the text of all tokens in this buffer.
 */
std::string BufferedTokenStream::getSourceName() const {
  return _tokenSource->getSourceName();
}

std::string BufferedTokenStream::getText() {
  return getText(misc::Interval(0U, size() - 1));
}

std::string BufferedTokenStream::getText(const misc::Interval& interval) {
  lazyInit();
  fill();
  size_t start = interval.a;
  size_t stop = interval.b;
  if (start == INVALID_INDEX || stop == INVALID_INDEX) {
    return "";
  }
  if (stop >= _tokens.size()) {
    stop = _tokens.size() - 1;
  }

  std::stringstream ss;
  for (size_t i = start; i <= stop; i++) {
    Token* t = _tokens[i].get();
    if (t->getType() == Token::EOF) {
      break;
    }
    ss << t->getText();
  }
  return ss.str();
}

std::string BufferedTokenStream::getText(RuleContext* ctx) {
  return getText(ctx->getSourceInterval());
}

std::string BufferedTokenStream::getText(Token* start, Token* stop) {
  if (start != nullptr && stop != nullptr) {
    return getText(
        misc::Interval(start->getTokenIndex(), stop->getTokenIndex()));
  }

  return "";
}

void BufferedTokenStream::fill() {
  lazyInit();
  const size_t blockSize = 1000;
  while (true) {
    size_t fetched = fetch(blockSize);
    if (fetched < blockSize) {
      return;
    }
  }
}

void BufferedTokenStream::InitializeInstanceFields() {
  _needSetup = true;
  _fetchedEOF = false;
}
