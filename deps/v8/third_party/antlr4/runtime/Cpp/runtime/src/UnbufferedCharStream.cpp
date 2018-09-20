/* Copyright (c) 2012-2017 The ANTLR Project. All rights reserved.
 * Use of this file is governed by the BSD 3-clause license that
 * can be found in the LICENSE.txt file in the project root.
 */

#include "Exceptions.h"
#include "misc/Interval.h"
#include "support/StringUtils.h"

#include "UnbufferedCharStream.h"

using namespace antlrcpp;
using namespace antlr4;
using namespace antlr4::misc;

UnbufferedCharStream::UnbufferedCharStream(std::wistream& input)
    : _input(input) {
  InitializeInstanceFields();

  // The vector's size is what used to be n in Java code.
  fill(1);  // prime
}

void UnbufferedCharStream::consume() {
  if (LA(1) == EOF) {
    throw IllegalStateException("cannot consume EOF");
  }

  // buf always has at least data[p==0] in this method due to ctor
  _lastChar = _data[_p];  // track last char for LA(-1)

  if (_p == _data.size() - 1 && _numMarkers == 0) {
    size_t capacity = _data.capacity();
    _data.clear();
    _data.reserve(capacity);

    _p = 0;
    _lastCharBufferStart = _lastChar;
  } else {
    _p++;
  }

  _currentCharIndex++;
  sync(1);
}

void UnbufferedCharStream::sync(size_t want) {
  if (_p + want <= _data.size())  // Already enough data loaded?
    return;

  fill(_p + want - _data.size());
}

size_t UnbufferedCharStream::fill(size_t n) {
  for (size_t i = 0; i < n; i++) {
    if (_data.size() > 0 && _data.back() == 0xFFFF) {
      return i;
    }

    try {
      char32_t c = nextChar();
      add(c);
#if defined(_MSC_FULL_VER) && _MSC_FULL_VER < 190023026
    } catch (IOException& ioe) {
      // throw_with_nested is not available before VS 2015.
      throw ioe;
#else
    } catch (IOException& /*ioe*/) {
      std::throw_with_nested(RuntimeException());
#endif
    }
  }

  return n;
}

char32_t UnbufferedCharStream::nextChar() {
  wchar_t result = 0;
  _input >> result;
  return result;
}

void UnbufferedCharStream::add(char32_t c) { _data += c; }

size_t UnbufferedCharStream::LA(ssize_t i) {
  if (i == -1) {  // special case
    return _lastChar;
  }

  // We can look back only as many chars as we have buffered.
  ssize_t index = static_cast<ssize_t>(_p) + i - 1;
  if (index < 0) {
    throw IndexOutOfBoundsException();
  }

  if (i > 0) {
    sync(static_cast<size_t>(i));  // No need to sync if we look back.
  }
  if (static_cast<size_t>(index) >= _data.size()) {
    return EOF;
  }

  if (_data[static_cast<size_t>(index)] == 0xFFFF) {
    return EOF;
  }

  return _data[static_cast<size_t>(index)];
}

ssize_t UnbufferedCharStream::mark() {
  if (_numMarkers == 0) {
    _lastCharBufferStart = _lastChar;
  }

  ssize_t mark = -static_cast<ssize_t>(_numMarkers) - 1;
  _numMarkers++;
  return mark;
}

void UnbufferedCharStream::release(ssize_t marker) {
  ssize_t expectedMark = -static_cast<ssize_t>(_numMarkers);
  if (marker != expectedMark) {
    throw IllegalStateException("release() called with an invalid marker.");
  }

  _numMarkers--;
  if (_numMarkers == 0 && _p > 0) {
    _data.erase(0, _p);
    _p = 0;
    _lastCharBufferStart = _lastChar;
  }
}

size_t UnbufferedCharStream::index() { return _currentCharIndex; }

void UnbufferedCharStream::seek(size_t index) {
  if (index == _currentCharIndex) {
    return;
  }

  if (index > _currentCharIndex) {
    sync(index - _currentCharIndex);
    index = std::min(index, getBufferStartIndex() + _data.size() - 1);
  }

  // index == to bufferStartIndex should set p to 0
  ssize_t i =
      static_cast<ssize_t>(index) - static_cast<ssize_t>(getBufferStartIndex());
  if (i < 0) {
    throw IllegalArgumentException(
        std::string("cannot seek to negative index ") + std::to_string(index));
  } else if (i >= static_cast<ssize_t>(_data.size())) {
    throw UnsupportedOperationException(
        "Seek to index outside buffer: " + std::to_string(index) + " not in " +
        std::to_string(getBufferStartIndex()) + ".." +
        std::to_string(getBufferStartIndex() + _data.size()));
  }

  _p = static_cast<size_t>(i);
  _currentCharIndex = index;
  if (_p == 0) {
    _lastChar = _lastCharBufferStart;
  } else {
    _lastChar = _data[_p - 1];
  }
}

size_t UnbufferedCharStream::size() {
  throw UnsupportedOperationException("Unbuffered stream cannot know its size");
}

std::string UnbufferedCharStream::getSourceName() const {
  if (name.empty()) {
    return UNKNOWN_SOURCE_NAME;
  }

  return name;
}

std::string UnbufferedCharStream::getText(const misc::Interval& interval) {
  if (interval.a < 0 || interval.b >= interval.a - 1) {
    throw IllegalArgumentException("invalid interval");
  }

  size_t bufferStartIndex = getBufferStartIndex();
  if (!_data.empty() && _data.back() == 0xFFFF) {
    if (interval.a + interval.length() > bufferStartIndex + _data.size()) {
      throw IllegalArgumentException(
          "the interval extends past the end of the stream");
    }
  }

  if (interval.a < static_cast<ssize_t>(bufferStartIndex) ||
      interval.b >= ssize_t(bufferStartIndex + _data.size())) {
    throw UnsupportedOperationException(
        "interval " + interval.toString() +
        " outside buffer: " + std::to_string(bufferStartIndex) + ".." +
        std::to_string(bufferStartIndex + _data.size() - 1));
  }
  // convert from absolute to local index
  size_t i = interval.a - bufferStartIndex;
  return utf32_to_utf8(_data.substr(i, interval.length()));
}

size_t UnbufferedCharStream::getBufferStartIndex() const {
  return _currentCharIndex - _p;
}

void UnbufferedCharStream::InitializeInstanceFields() {
  _p = 0;
  _numMarkers = 0;
  _lastChar = 0;
  _lastCharBufferStart = 0;
  _currentCharIndex = 0;
}
