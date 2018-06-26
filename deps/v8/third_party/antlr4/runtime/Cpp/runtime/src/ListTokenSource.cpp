/* Copyright (c) 2012-2017 The ANTLR Project. All rights reserved.
 * Use of this file is governed by the BSD 3-clause license that
 * can be found in the LICENSE.txt file in the project root.
 */

#include "CharStream.h"
#include "CommonToken.h"
#include "Token.h"

#include "ListTokenSource.h"

using namespace antlr4;

ListTokenSource::ListTokenSource(std::vector<std::unique_ptr<Token>> tokens_)
    : ListTokenSource(std::move(tokens_), "") {}

ListTokenSource::ListTokenSource(std::vector<std::unique_ptr<Token>> tokens_,
                                 const std::string& sourceName_)
    : tokens(std::move(tokens_)), sourceName(sourceName_) {
  InitializeInstanceFields();
  if (tokens.empty()) {
    throw "tokens cannot be null";
  }

  // Check if there is an eof token and create one if not.
  if (tokens.back()->getType() != Token::EOF) {
    Token* lastToken = tokens.back().get();
    size_t start = INVALID_INDEX;
    size_t previousStop = lastToken->getStopIndex();
    if (previousStop != INVALID_INDEX) {
      start = previousStop + 1;
    }

    size_t stop = std::max(INVALID_INDEX, start - 1);
    tokens.emplace_back((_factory->create(
        {this, getInputStream()}, Token::EOF, "EOF", Token::DEFAULT_CHANNEL,
        start, stop, static_cast<int>(lastToken->getLine()),
        lastToken->getCharPositionInLine())));
  }
}

size_t ListTokenSource::getCharPositionInLine() {
  if (i < tokens.size()) {
    return tokens[i]->getCharPositionInLine();
  }
  return 0;
}

std::unique_ptr<Token> ListTokenSource::nextToken() {
  if (i < tokens.size()) {
    return std::move(tokens[i++]);
  }
  return nullptr;
}

size_t ListTokenSource::getLine() const {
  if (i < tokens.size()) {
    return tokens[i]->getLine();
  }

  return 1;
}

CharStream* ListTokenSource::getInputStream() {
  if (i < tokens.size()) {
    return tokens[i]->getInputStream();
  } else if (!tokens.empty()) {
    return tokens.back()->getInputStream();
  }

  // no input stream information is available
  return nullptr;
}

std::string ListTokenSource::getSourceName() {
  if (sourceName != "") {
    return sourceName;
  }

  CharStream* inputStream = getInputStream();
  if (inputStream != nullptr) {
    return inputStream->getSourceName();
  }

  return "List";
}

Ref<TokenFactory<CommonToken>> ListTokenSource::getTokenFactory() {
  return _factory;
}

void ListTokenSource::InitializeInstanceFields() {
  i = 0;
  _factory = CommonTokenFactory::DEFAULT;
}
