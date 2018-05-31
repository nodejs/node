/* Copyright (c) 2012-2017 The ANTLR Project. All rights reserved.
 * Use of this file is governed by the BSD 3-clause license that
 * can be found in the LICENSE.txt file in the project root.
 */

#include "Exceptions.h"
#include "RuleContext.h"
#include "Token.h"
#include "TokenSource.h"
#include "WritableToken.h"
#include "assert.h"
#include "misc/Interval.h"
#include "support/Arrays.h"

#include "UnbufferedTokenStream.h"

using namespace antlr4;

UnbufferedTokenStream::UnbufferedTokenStream(TokenSource* tokenSource)
    : UnbufferedTokenStream(tokenSource, 256) {}

UnbufferedTokenStream::UnbufferedTokenStream(TokenSource* tokenSource,
                                             int /*bufferSize*/)
    : _tokenSource(tokenSource),
      _lastToken(nullptr),
      _lastTokenBufferStart(nullptr) {
  InitializeInstanceFields();
  fill(1);  // prime the pump
}

UnbufferedTokenStream::~UnbufferedTokenStream() {}

Token* UnbufferedTokenStream::get(size_t i) const {  // get absolute index
  size_t bufferStartIndex = getBufferStartIndex();
  if (i < bufferStartIndex || i >= bufferStartIndex + _tokens.size()) {
    throw IndexOutOfBoundsException(
        std::string("get(") + std::to_string(i) +
        std::string(") outside buffer: ") + std::to_string(bufferStartIndex) +
        std::string("..") + std::to_string(bufferStartIndex + _tokens.size()));
  }
  return _tokens[i - bufferStartIndex].get();
}

Token* UnbufferedTokenStream::LT(ssize_t i) {
  if (i == -1) {
    return _lastToken;
  }

  sync(i);
  ssize_t index = static_cast<ssize_t>(_p) + i - 1;
  if (index < 0) {
    throw IndexOutOfBoundsException(std::string("LT(") + std::to_string(i) +
                                    std::string(") gives negative index"));
  }

  if (index >= static_cast<ssize_t>(_tokens.size())) {
    assert(_tokens.size() > 0 && _tokens.back()->getType() == EOF);
    return _tokens.back().get();
  }

  return _tokens[static_cast<size_t>(index)].get();
}

size_t UnbufferedTokenStream::LA(ssize_t i) { return LT(i)->getType(); }

TokenSource* UnbufferedTokenStream::getTokenSource() const {
  return _tokenSource;
}

std::string UnbufferedTokenStream::getText() { return ""; }

std::string UnbufferedTokenStream::getText(RuleContext* ctx) {
  return getText(ctx->getSourceInterval());
}

std::string UnbufferedTokenStream::getText(Token* start, Token* stop) {
  return getText(misc::Interval(start->getTokenIndex(), stop->getTokenIndex()));
}

void UnbufferedTokenStream::consume() {
  if (LA(1) == EOF) {
    throw IllegalStateException("cannot consume EOF");
  }

  // buf always has at least tokens[p==0] in this method due to ctor
  _lastToken = _tokens[_p].get();  // track last token for LT(-1)

  // if we're at last token and no markers, opportunity to flush buffer
  if (_p == _tokens.size() - 1 && _numMarkers == 0) {
    _tokens.clear();
    _p = 0;
    _lastTokenBufferStart = _lastToken;
  } else {
    ++_p;
  }

  ++_currentTokenIndex;
  sync(1);
}

/// <summary>
/// Make sure we have 'need' elements from current position <seealso cref="#p
/// p"/>. Last valid
///  {@code p} index is {@code tokens.length-1}.  {@code p+need-1} is the tokens
///  index 'need' elements ahead.  If we need 1 element, {@code (p+1-1)==p} must
///  be less than {@code tokens.length}.
/// </summary>
void UnbufferedTokenStream::sync(ssize_t want) {
  ssize_t need = (static_cast<ssize_t>(_p) + want - 1) -
                 static_cast<ssize_t>(_tokens.size()) +
                 1;  // how many more elements we need?
  if (need > 0) {
    fill(static_cast<size_t>(need));
  }
}

/// <summary>
/// Add {@code n} elements to the buffer. Returns the number of tokens
/// actually added to the buffer. If the return value is less than {@code n},
/// then EOF was reached before {@code n} tokens could be added.
/// </summary>
size_t UnbufferedTokenStream::fill(size_t n) {
  for (size_t i = 0; i < n; i++) {
    if (_tokens.size() > 0 && _tokens.back()->getType() == EOF) {
      return i;
    }

    add(_tokenSource->nextToken());
  }

  return n;
}

void UnbufferedTokenStream::add(std::unique_ptr<Token> t) {
  WritableToken* writable = dynamic_cast<WritableToken*>(t.get());
  if (writable != nullptr) {
    writable->setTokenIndex(int(getBufferStartIndex() + _tokens.size()));
  }

  _tokens.push_back(std::move(t));
}

/// <summary>
/// Return a marker that we can release later.
/// <p/>
/// The specific marker value used for this class allows for some level of
/// protection against misuse where {@code seek()} is called on a mark or
/// {@code release()} is called in the wrong order.
/// </summary>
ssize_t UnbufferedTokenStream::mark() {
  if (_numMarkers == 0) {
    _lastTokenBufferStart = _lastToken;
  }

  int mark = -_numMarkers - 1;
  _numMarkers++;
  return mark;
}

void UnbufferedTokenStream::release(ssize_t marker) {
  ssize_t expectedMark = -_numMarkers;
  if (marker != expectedMark) {
    throw IllegalStateException("release() called with an invalid marker.");
  }

  _numMarkers--;
  if (_numMarkers == 0) {  // can we release buffer?
    if (_p > 0) {
      // Copy tokens[p]..tokens[n-1] to tokens[0]..tokens[(n-1)-p], reset ptrs
      // p is last valid token; move nothing if p==n as we have no valid char
      _tokens.erase(_tokens.begin(),
                    _tokens.begin() + static_cast<ssize_t>(_p));
      _p = 0;
    }

    _lastTokenBufferStart = _lastToken;
  }
}

size_t UnbufferedTokenStream::index() { return _currentTokenIndex; }

void UnbufferedTokenStream::seek(size_t index) {  // seek to absolute index
  if (index == _currentTokenIndex) {
    return;
  }

  if (index > _currentTokenIndex) {
    sync(ssize_t(index - _currentTokenIndex));
    index = std::min(index, getBufferStartIndex() + _tokens.size() - 1);
  }

  size_t bufferStartIndex = getBufferStartIndex();
  if (bufferStartIndex > index) {
    throw IllegalArgumentException(
        std::string("cannot seek to negative index ") + std::to_string(index));
  }

  size_t i = index - bufferStartIndex;
  if (i >= _tokens.size()) {
    throw UnsupportedOperationException(
        std::string("seek to index outside buffer: ") + std::to_string(index) +
        " not in " + std::to_string(bufferStartIndex) + ".." +
        std::to_string(bufferStartIndex + _tokens.size()));
  }

  _p = i;
  _currentTokenIndex = index;
  if (_p == 0) {
    _lastToken = _lastTokenBufferStart;
  } else {
    _lastToken = _tokens[_p - 1].get();
  }
}

size_t UnbufferedTokenStream::size() {
  throw UnsupportedOperationException("Unbuffered stream cannot know its size");
}

std::string UnbufferedTokenStream::getSourceName() const {
  return _tokenSource->getSourceName();
}

std::string UnbufferedTokenStream::getText(const misc::Interval& interval) {
  size_t bufferStartIndex = getBufferStartIndex();
  size_t bufferStopIndex = bufferStartIndex + _tokens.size() - 1;

  size_t start = interval.a;
  size_t stop = interval.b;
  if (start < bufferStartIndex || stop > bufferStopIndex) {
    throw UnsupportedOperationException(
        std::string("interval ") + interval.toString() +
        " not in token buffer window: " + std::to_string(bufferStartIndex) +
        ".." + std::to_string(bufferStopIndex));
  }

  size_t a = start - bufferStartIndex;
  size_t b = stop - bufferStartIndex;

  std::stringstream ss;
  for (size_t i = a; i <= b; i++) {
    Token* t = _tokens[i].get();
    if (i > 0) ss << ", ";
    ss << t->getText();
  }

  return ss.str();
}

size_t UnbufferedTokenStream::getBufferStartIndex() const {
  return _currentTokenIndex - _p;
}

void UnbufferedTokenStream::InitializeInstanceFields() {
  _p = 0;
  _numMarkers = 0;
  _currentTokenIndex = 0;
}
