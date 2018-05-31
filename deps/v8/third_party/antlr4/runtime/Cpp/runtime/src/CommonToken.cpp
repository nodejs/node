/* Copyright (c) 2012-2017 The ANTLR Project. All rights reserved.
 * Use of this file is governed by the BSD 3-clause license that
 * can be found in the LICENSE.txt file in the project root.
 */

#include "CharStream.h"
#include "Recognizer.h"
#include "TokenSource.h"
#include "Vocabulary.h"

#include "misc/Interval.h"

#include "support/CPPUtils.h"
#include "support/StringUtils.h"

#include "CommonToken.h"

using namespace antlr4;
using namespace antlr4::misc;

using namespace antlrcpp;

const std::pair<TokenSource*, CharStream*> CommonToken::EMPTY_SOURCE(nullptr,
                                                                     nullptr);

CommonToken::CommonToken(size_t type) {
  InitializeInstanceFields();
  _type = type;
}

CommonToken::CommonToken(std::pair<TokenSource*, CharStream*> source,
                         size_t type, size_t channel, size_t start,
                         size_t stop) {
  InitializeInstanceFields();
  _source = source;
  _type = type;
  _channel = channel;
  _start = start;
  _stop = stop;
  if (_source.first != nullptr) {
    _line = static_cast<int>(source.first->getLine());
    _charPositionInLine = source.first->getCharPositionInLine();
  }
}

CommonToken::CommonToken(size_t type, const std::string& text) {
  InitializeInstanceFields();
  _type = type;
  _channel = DEFAULT_CHANNEL;
  _text = text;
  _source = EMPTY_SOURCE;
}

CommonToken::CommonToken(Token* oldToken) {
  InitializeInstanceFields();
  _type = oldToken->getType();
  _line = oldToken->getLine();
  _index = oldToken->getTokenIndex();
  _charPositionInLine = oldToken->getCharPositionInLine();
  _channel = oldToken->getChannel();
  _start = oldToken->getStartIndex();
  _stop = oldToken->getStopIndex();

  if (is<CommonToken*>(oldToken)) {
    _text = (static_cast<CommonToken*>(oldToken))->_text;
    _source = (static_cast<CommonToken*>(oldToken))->_source;
  } else {
    _text = oldToken->getText();
    _source = {oldToken->getTokenSource(), oldToken->getInputStream()};
  }
}

size_t CommonToken::getType() const { return _type; }

void CommonToken::setLine(size_t line) { _line = line; }

std::string CommonToken::getText() const {
  if (!_text.empty()) {
    return _text;
  }

  CharStream* input = getInputStream();
  if (input == nullptr) {
    return "";
  }
  size_t n = input->size();
  if (_start < n && _stop < n) {
    return input->getText(misc::Interval(_start, _stop));
  } else {
    return "<EOF>";
  }
}

void CommonToken::setText(const std::string& text) { _text = text; }

size_t CommonToken::getLine() const { return _line; }

size_t CommonToken::getCharPositionInLine() const {
  return _charPositionInLine;
}

void CommonToken::setCharPositionInLine(size_t charPositionInLine) {
  _charPositionInLine = charPositionInLine;
}

size_t CommonToken::getChannel() const { return _channel; }

void CommonToken::setChannel(size_t channel) { _channel = channel; }

void CommonToken::setType(size_t type) { _type = type; }

size_t CommonToken::getStartIndex() const { return _start; }

void CommonToken::setStartIndex(size_t start) { _start = start; }

size_t CommonToken::getStopIndex() const { return _stop; }

void CommonToken::setStopIndex(size_t stop) { _stop = stop; }

size_t CommonToken::getTokenIndex() const { return _index; }

void CommonToken::setTokenIndex(size_t index) { _index = index; }

antlr4::TokenSource* CommonToken::getTokenSource() const {
  return _source.first;
}

antlr4::CharStream* CommonToken::getInputStream() const {
  return _source.second;
}

std::string CommonToken::toString() const { return toString(nullptr); }

std::string CommonToken::toString(Recognizer* r) const {
  std::stringstream ss;

  std::string channelStr;
  if (_channel > 0) {
    channelStr = ",channel=" + std::to_string(_channel);
  }
  std::string txt = getText();
  if (!txt.empty()) {
    antlrcpp::replaceAll(txt, "\n", "\\n");
    antlrcpp::replaceAll(txt, "\r", "\\r");
    antlrcpp::replaceAll(txt, "\t", "\\t");
  } else {
    txt = "<no text>";
  }

  std::string typeString = std::to_string(symbolToNumeric(_type));
  if (r != nullptr) typeString = r->getVocabulary().getDisplayName(_type);

  ss << "[@" << symbolToNumeric(getTokenIndex()) << ","
     << symbolToNumeric(_start) << ":" << symbolToNumeric(_stop) << "='" << txt
     << "',<" << typeString << ">" << channelStr << "," << _line << ":"
     << getCharPositionInLine() << "]";

  return ss.str();
}

void CommonToken::InitializeInstanceFields() {
  _type = 0;
  _line = 0;
  _charPositionInLine = INVALID_INDEX;
  _channel = DEFAULT_CHANNEL;
  _index = INVALID_INDEX;
  _start = 0;
  _stop = 0;
  _source = EMPTY_SOURCE;
}
