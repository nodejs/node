/* Copyright (c) 2012-2017 The ANTLR Project. All rights reserved.
 * Use of this file is governed by the BSD 3-clause license that
 * can be found in the LICENSE.txt file in the project root.
 */

#include "ConsoleErrorListener.h"
#include "RecognitionException.h"
#include "Token.h"
#include "atn/ATN.h"
#include "atn/ATNSimulator.h"
#include "support/CPPUtils.h"
#include "support/StringUtils.h"

#include "Vocabulary.h"

#include "Recognizer.h"

using namespace antlr4;
using namespace antlr4::atn;

std::map<const dfa::Vocabulary*, std::map<std::string, size_t>>
    Recognizer::_tokenTypeMapCache;
std::map<std::vector<std::string>, std::map<std::string, size_t>>
    Recognizer::_ruleIndexMapCache;

Recognizer::Recognizer() {
  InitializeInstanceFields();
  _proxListener.addErrorListener(&ConsoleErrorListener::INSTANCE);
}

Recognizer::~Recognizer() {}

dfa::Vocabulary const& Recognizer::getVocabulary() const {
  static dfa::Vocabulary vocabulary =
      dfa::Vocabulary::fromTokenNames(getTokenNames());
  return vocabulary;
}

std::map<std::string, size_t> Recognizer::getTokenTypeMap() {
  const dfa::Vocabulary& vocabulary = getVocabulary();

  std::lock_guard<std::mutex> lck(_mutex);
  std::map<std::string, size_t> result;
  auto iterator = _tokenTypeMapCache.find(&vocabulary);
  if (iterator != _tokenTypeMapCache.end()) {
    result = iterator->second;
  } else {
    for (size_t i = 0; i <= getATN().maxTokenType; ++i) {
      std::string literalName = vocabulary.getLiteralName(i);
      if (!literalName.empty()) {
        result[literalName] = i;
      }

      std::string symbolicName = vocabulary.getSymbolicName(i);
      if (!symbolicName.empty()) {
        result[symbolicName] = i;
      }
    }
    result["EOF"] = EOF;
    _tokenTypeMapCache[&vocabulary] = result;
  }

  return result;
}

std::map<std::string, size_t> Recognizer::getRuleIndexMap() {
  const std::vector<std::string>& ruleNames = getRuleNames();
  if (ruleNames.empty()) {
    throw "The current recognizer does not provide a list of rule names.";
  }

  std::lock_guard<std::mutex> lck(_mutex);
  std::map<std::string, size_t> result;
  auto iterator = _ruleIndexMapCache.find(ruleNames);
  if (iterator != _ruleIndexMapCache.end()) {
    result = iterator->second;
  } else {
    result = antlrcpp::toMap(ruleNames);
    _ruleIndexMapCache[ruleNames] = result;
  }
  return result;
}

size_t Recognizer::getTokenType(const std::string& tokenName) {
  const std::map<std::string, size_t>& map = getTokenTypeMap();
  auto iterator = map.find(tokenName);
  if (iterator == map.end()) return Token::INVALID_TYPE;

  return iterator->second;
}

void Recognizer::setInterpreter(atn::ATNSimulator* interpreter) {
  // Usually the interpreter is set by the descendant (lexer or parser
  // (simulator), but can also be exchanged by the profiling ATN simulator.
  delete _interpreter;
  _interpreter = interpreter;
}

std::string Recognizer::getErrorHeader(RecognitionException* e) {
  // We're having issues with cross header dependencies, these two classes will
  // need to be rewritten to remove that.
  size_t line = e->getOffendingToken()->getLine();
  size_t charPositionInLine = e->getOffendingToken()->getCharPositionInLine();
  return std::string("line ") + std::to_string(line) + ":" +
         std::to_string(charPositionInLine);
}

std::string Recognizer::getTokenErrorDisplay(Token* t) {
  if (t == nullptr) {
    return "<no Token>";
  }
  std::string s = t->getText();
  if (s == "") {
    if (t->getType() == EOF) {
      s = "<EOF>";
    } else {
      s = std::string("<") + std::to_string(t->getType()) + std::string(">");
    }
  }

  antlrcpp::replaceAll(s, "\n", "\\n");
  antlrcpp::replaceAll(s, "\r", "\\r");
  antlrcpp::replaceAll(s, "\t", "\\t");

  return "'" + s + "'";
}

void Recognizer::addErrorListener(ANTLRErrorListener* listener) {
  _proxListener.addErrorListener(listener);
}

void Recognizer::removeErrorListener(ANTLRErrorListener* listener) {
  _proxListener.removeErrorListener(listener);
}

void Recognizer::removeErrorListeners() {
  _proxListener.removeErrorListeners();
}

ProxyErrorListener& Recognizer::getErrorListenerDispatch() {
  return _proxListener;
}

bool Recognizer::sempred(RuleContext* /*localctx*/, size_t /*ruleIndex*/,
                         size_t /*actionIndex*/) {
  return true;
}

bool Recognizer::precpred(RuleContext* /*localctx*/, int /*precedence*/) {
  return true;
}

void Recognizer::action(RuleContext* /*localctx*/, size_t /*ruleIndex*/,
                        size_t /*actionIndex*/) {}

size_t Recognizer::getState() const { return _stateNumber; }

void Recognizer::setState(size_t atnState) { _stateNumber = atnState; }

void Recognizer::InitializeInstanceFields() {
  _stateNumber = ATNState::INVALID_STATE_NUMBER;
  _interpreter = nullptr;
}
