/* Copyright (c) 2012-2017 The ANTLR Project. All rights reserved.
 * Use of this file is governed by the BSD 3-clause license that
 * can be found in the LICENSE.txt file in the project root.
 */

#include "Exceptions.h"
#include "IntStream.h"
#include "misc/Interval.h"

#include "support/CPPUtils.h"
#include "support/StringUtils.h"

#include "ANTLRInputStream.h"

using namespace antlr4;
using namespace antlrcpp;

using misc::Interval;

ANTLRInputStream::ANTLRInputStream(const std::string& input) {
  InitializeInstanceFields();
  load(input);
}

ANTLRInputStream::ANTLRInputStream(const char data_[],
                                   size_t numberOfActualCharsInArray)
    : ANTLRInputStream(std::string(data_, numberOfActualCharsInArray)) {}

ANTLRInputStream::ANTLRInputStream(std::istream& stream) {
  InitializeInstanceFields();
  load(stream);
}

void ANTLRInputStream::load(const std::string& input) {
  // Remove the UTF-8 BOM if present.
  const char bom[4] = "\xef\xbb\xbf";
  if (input.compare(0, 3, bom, 3) == 0)
    _data =
        antlrcpp::utf8_to_utf32(input.data() + 3, input.data() + input.size());
  else
    _data = antlrcpp::utf8_to_utf32(input.data(), input.data() + input.size());
  p = 0;
}

void ANTLRInputStream::load(std::istream& stream) {
  if (!stream.good() || stream.eof())  // No fail, bad or EOF.
    return;

  _data.clear();

  std::string s((std::istreambuf_iterator<char>(stream)),
                std::istreambuf_iterator<char>());
  load(s);
}

void ANTLRInputStream::reset() { p = 0; }

void ANTLRInputStream::consume() {
  if (p >= _data.size()) {
    assert(LA(1) == IntStream::EOF);
    throw IllegalStateException("cannot consume EOF");
  }

  if (p < _data.size()) {
    p++;
  }
}

size_t ANTLRInputStream::LA(ssize_t i) {
  if (i == 0) {
    return 0;  // undefined
  }

  ssize_t position = static_cast<ssize_t>(p);
  if (i < 0) {
    i++;  // e.g., translate LA(-1) to use offset i=0; then _data[p+0-1]
    if ((position + i - 1) < 0) {
      return IntStream::EOF;  // invalid; no char before first char
    }
  }

  if ((position + i - 1) >= static_cast<ssize_t>(_data.size())) {
    return IntStream::EOF;
  }

  return _data[static_cast<size_t>((position + i - 1))];
}

size_t ANTLRInputStream::LT(ssize_t i) { return LA(i); }

size_t ANTLRInputStream::index() { return p; }

size_t ANTLRInputStream::size() { return _data.size(); }

// Mark/release do nothing. We have entire buffer.
ssize_t ANTLRInputStream::mark() { return -1; }

void ANTLRInputStream::release(ssize_t /* marker */) {}

void ANTLRInputStream::seek(size_t index) {
  if (index <= p) {
    p = index;  // just jump; don't update stream state (line, ...)
    return;
  }
  // seek forward, consume until p hits index or n (whichever comes first)
  index = std::min(index, _data.size());
  while (p < index) {
    consume();
  }
}

std::string ANTLRInputStream::getText(const Interval& interval) {
  if (interval.a < 0 || interval.b < 0) {
    return "";
  }

  size_t start = static_cast<size_t>(interval.a);
  size_t stop = static_cast<size_t>(interval.b);

  if (stop >= _data.size()) {
    stop = _data.size() - 1;
  }

  size_t count = stop - start + 1;
  if (start >= _data.size()) {
    return "";
  }

  return antlrcpp::utf32_to_utf8(_data.substr(start, count));
}

std::string ANTLRInputStream::getSourceName() const {
  if (name.empty()) {
    return IntStream::UNKNOWN_SOURCE_NAME;
  }
  return name;
}

std::string ANTLRInputStream::toString() const {
  return antlrcpp::utf32_to_utf8(_data);
}

void ANTLRInputStream::InitializeInstanceFields() { p = 0; }
