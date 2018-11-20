/* Copyright (c) 2012-2017 The ANTLR Project. All rights reserved.
 * Use of this file is governed by the BSD 3-clause license that
 * can be found in the LICENSE.txt file in the project root.
 */

#include "Token.h"

#include "Vocabulary.h"

using namespace antlr4::dfa;

const Vocabulary Vocabulary::EMPTY_VOCABULARY;

Vocabulary::Vocabulary(const std::vector<std::string>& literalNames,
                       const std::vector<std::string>& symbolicNames)
    : Vocabulary(literalNames, symbolicNames, {}) {}

Vocabulary::Vocabulary(const std::vector<std::string>& literalNames,
                       const std::vector<std::string>& symbolicNames,
                       const std::vector<std::string>& displayNames)
    : _literalNames(literalNames),
      _symbolicNames(symbolicNames),
      _displayNames(displayNames),
      _maxTokenType(
          std::max(_displayNames.size(),
                   std::max(_literalNames.size(), _symbolicNames.size())) -
          1) {
  // See note here on -1 part: https://github.com/antlr/antlr4/pull/1146
}

Vocabulary::~Vocabulary() {}

Vocabulary Vocabulary::fromTokenNames(
    const std::vector<std::string>& tokenNames) {
  if (tokenNames.empty()) {
    return EMPTY_VOCABULARY;
  }

  std::vector<std::string> literalNames = tokenNames;
  std::vector<std::string> symbolicNames = tokenNames;
  std::locale locale;
  for (size_t i = 0; i < tokenNames.size(); i++) {
    std::string tokenName = tokenNames[i];
    if (tokenName == "") {
      continue;
    }

    if (!tokenName.empty()) {
      char firstChar = tokenName[0];
      if (firstChar == '\'') {
        symbolicNames[i] = "";
        continue;
      } else if (std::isupper(firstChar, locale)) {
        literalNames[i] = "";
        continue;
      }
    }

    // wasn't a literal or symbolic name
    literalNames[i] = "";
    symbolicNames[i] = "";
  }

  return Vocabulary(literalNames, symbolicNames, tokenNames);
}

size_t Vocabulary::getMaxTokenType() const { return _maxTokenType; }

std::string Vocabulary::getLiteralName(size_t tokenType) const {
  if (tokenType < _literalNames.size()) {
    return _literalNames[tokenType];
  }

  return "";
}

std::string Vocabulary::getSymbolicName(size_t tokenType) const {
  if (tokenType == Token::EOF) {
    return "EOF";
  }

  if (tokenType < _symbolicNames.size()) {
    return _symbolicNames[tokenType];
  }

  return "";
}

std::string Vocabulary::getDisplayName(size_t tokenType) const {
  if (tokenType < _displayNames.size()) {
    std::string displayName = _displayNames[tokenType];
    if (!displayName.empty()) {
      return displayName;
    }
  }

  std::string literalName = getLiteralName(tokenType);
  if (!literalName.empty()) {
    return literalName;
  }

  std::string symbolicName = getSymbolicName(tokenType);
  if (!symbolicName.empty()) {
    return symbolicName;
  }

  return std::to_string(tokenType);
}
