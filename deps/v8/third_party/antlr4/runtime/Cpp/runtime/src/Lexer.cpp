/* Copyright (c) 2012-2017 The ANTLR Project. All rights reserved.
 * Use of this file is governed by the BSD 3-clause license that
 * can be found in the LICENSE.txt file in the project root.
 */

#include "ANTLRErrorListener.h"
#include "CommonToken.h"
#include "CommonTokenFactory.h"
#include "Exceptions.h"
#include "LexerNoViableAltException.h"
#include "atn/LexerATNSimulator.h"
#include "misc/Interval.h"
#include "support/CPPUtils.h"
#include "support/StringUtils.h"

#include "Lexer.h"

#define DEBUG_LEXER 0

using namespace antlrcpp;
using namespace antlr4;

Lexer::Lexer() : Recognizer() {
  InitializeInstanceFields();
  _input = nullptr;
}

Lexer::Lexer(CharStream* input) : Recognizer(), _input(input) {
  InitializeInstanceFields();
}

void Lexer::reset() {
  // wack Lexer state variables
  _input->seek(0);  // rewind the input

  _syntaxErrors = 0;
  token.reset();
  type = Token::INVALID_TYPE;
  channel = Token::DEFAULT_CHANNEL;
  tokenStartCharIndex = INVALID_INDEX;
  tokenStartCharPositionInLine = 0;
  tokenStartLine = 0;
  type = 0;
  _text = "";

  hitEOF = false;
  mode = Lexer::DEFAULT_MODE;
  modeStack.clear();

  getInterpreter<atn::LexerATNSimulator>()->reset();
}

std::unique_ptr<Token> Lexer::nextToken() {
  // Mark start location in char stream so unbuffered streams are
  // guaranteed at least have text of current token
  ssize_t tokenStartMarker = _input->mark();

  auto onExit = finally([this, tokenStartMarker] {
    // make sure we release marker after match or
    // unbuffered char stream will keep buffering
    _input->release(tokenStartMarker);
  });

  while (true) {
  outerContinue:
    if (hitEOF) {
      emitEOF();
      return std::move(token);
    }

    token.reset();
    channel = Token::DEFAULT_CHANNEL;
    tokenStartCharIndex = _input->index();
    tokenStartCharPositionInLine =
        getInterpreter<atn::LexerATNSimulator>()->getCharPositionInLine();
    tokenStartLine = getInterpreter<atn::LexerATNSimulator>()->getLine();
    _text = "";
    do {
      type = Token::INVALID_TYPE;
      size_t ttype;
      try {
        ttype = getInterpreter<atn::LexerATNSimulator>()->match(_input, mode);
      } catch (LexerNoViableAltException& e) {
        notifyListeners(e);  // report error
        recover(e);
        ttype = SKIP;
      }
      if (_input->LA(1) == EOF) {
        hitEOF = true;
      }
      if (type == Token::INVALID_TYPE) {
        type = ttype;
      }
      if (type == SKIP) {
        goto outerContinue;
      }
    } while (type == MORE);
    if (token == nullptr) {
      emit();
    }
    return std::move(token);
  }
}

void Lexer::skip() { type = SKIP; }

void Lexer::more() { type = MORE; }

void Lexer::setMode(size_t m) { mode = m; }

void Lexer::pushMode(size_t m) {
#if DEBUG_LEXER == 1
  std::cout << "pushMode " << m << std::endl;
#endif

  modeStack.push_back(mode);
  setMode(m);
}

size_t Lexer::popMode() {
  if (modeStack.empty()) {
    throw EmptyStackException();
  }
#if DEBUG_LEXER == 1
  std::cout << std::string("popMode back to ") << modeStack.back() << std::endl;
#endif

  setMode(modeStack.back());
  modeStack.pop_back();
  return mode;
}

Ref<TokenFactory<CommonToken>> Lexer::getTokenFactory() { return _factory; }

void Lexer::setInputStream(IntStream* input) {
  reset();
  _input = dynamic_cast<CharStream*>(input);
}

std::string Lexer::getSourceName() { return _input->getSourceName(); }

CharStream* Lexer::getInputStream() { return _input; }

void Lexer::emit(std::unique_ptr<Token> newToken) {
  token = std::move(newToken);
}

Token* Lexer::emit() {
  emit(_factory->create({this, _input}, type, _text, channel,
                        tokenStartCharIndex, getCharIndex() - 1, tokenStartLine,
                        tokenStartCharPositionInLine));
  return token.get();
}

Token* Lexer::emitEOF() {
  size_t cpos = getCharPositionInLine();
  size_t line = getLine();
  emit(_factory->create({this, _input}, EOF, "", Token::DEFAULT_CHANNEL,
                        _input->index(), _input->index() - 1, line, cpos));
  return token.get();
}

size_t Lexer::getLine() const {
  return getInterpreter<atn::LexerATNSimulator>()->getLine();
}

size_t Lexer::getCharPositionInLine() {
  return getInterpreter<atn::LexerATNSimulator>()->getCharPositionInLine();
}

void Lexer::setLine(size_t line) {
  getInterpreter<atn::LexerATNSimulator>()->setLine(line);
}

void Lexer::setCharPositionInLine(size_t charPositionInLine) {
  getInterpreter<atn::LexerATNSimulator>()->setCharPositionInLine(
      charPositionInLine);
}

size_t Lexer::getCharIndex() { return _input->index(); }

std::string Lexer::getText() {
  if (!_text.empty()) {
    return _text;
  }
  return getInterpreter<atn::LexerATNSimulator>()->getText(_input);
}

void Lexer::setText(const std::string& text) { _text = text; }

std::unique_ptr<Token> Lexer::getToken() { return std::move(token); }

void Lexer::setToken(std::unique_ptr<Token> newToken) {
  token = std::move(newToken);
}

void Lexer::setType(size_t ttype) { type = ttype; }

size_t Lexer::getType() { return type; }

void Lexer::setChannel(size_t newChannel) { channel = newChannel; }

size_t Lexer::getChannel() { return channel; }

std::vector<std::unique_ptr<Token>> Lexer::getAllTokens() {
  std::vector<std::unique_ptr<Token>> tokens;
  std::unique_ptr<Token> t = nextToken();
  while (t->getType() != EOF) {
    tokens.push_back(std::move(t));
    t = nextToken();
  }
  return tokens;
}

void Lexer::recover(const LexerNoViableAltException& /*e*/) {
  if (_input->LA(1) != EOF) {
    // skip a char and try again
    getInterpreter<atn::LexerATNSimulator>()->consume(_input);
  }
}

void Lexer::notifyListeners(const LexerNoViableAltException& /*e*/) {
  ++_syntaxErrors;
  std::string text =
      _input->getText(misc::Interval(tokenStartCharIndex, _input->index()));
  std::string msg = std::string("token recognition error at: '") +
                    getErrorDisplay(text) + std::string("'");

  ProxyErrorListener& listener = getErrorListenerDispatch();
  listener.syntaxError(this, nullptr, tokenStartLine,
                       tokenStartCharPositionInLine, msg,
                       std::current_exception());
}

std::string Lexer::getErrorDisplay(const std::string& s) {
  std::stringstream ss;
  for (auto c : s) {
    switch (c) {
      case '\n':
        ss << "\\n";
        break;
      case '\t':
        ss << "\\t";
        break;
      case '\r':
        ss << "\\r";
        break;
      default:
        ss << c;
        break;
    }
  }
  return ss.str();
}

void Lexer::recover(RecognitionException* /*re*/) {
  // TO_DO: Do we lose character or line position information?
  _input->consume();
}

size_t Lexer::getNumberOfSyntaxErrors() { return _syntaxErrors; }

void Lexer::InitializeInstanceFields() {
  _syntaxErrors = 0;
  token = nullptr;
  _factory = CommonTokenFactory::DEFAULT;
  tokenStartCharIndex = INVALID_INDEX;
  tokenStartLine = 0;
  tokenStartCharPositionInLine = 0;
  hitEOF = false;
  channel = 0;
  type = 0;
  mode = Lexer::DEFAULT_MODE;
}
