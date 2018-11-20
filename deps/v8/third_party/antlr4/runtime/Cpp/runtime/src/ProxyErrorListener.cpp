/* Copyright (c) 2012-2017 The ANTLR Project. All rights reserved.
 * Use of this file is governed by the BSD 3-clause license that
 * can be found in the LICENSE.txt file in the project root.
 */

#include "ProxyErrorListener.h"

using namespace antlr4;

void ProxyErrorListener::addErrorListener(ANTLRErrorListener* listener) {
  if (listener == nullptr) {
    throw "listener cannot be null.";
  }

  _delegates.insert(listener);
}

void ProxyErrorListener::removeErrorListener(ANTLRErrorListener* listener) {
  _delegates.erase(listener);
}

void ProxyErrorListener::removeErrorListeners() { _delegates.clear(); }

void ProxyErrorListener::syntaxError(Recognizer* recognizer,
                                     Token* offendingSymbol, size_t line,
                                     size_t charPositionInLine,
                                     const std::string& msg,
                                     std::exception_ptr e) {
  for (auto listener : _delegates) {
    listener->syntaxError(recognizer, offendingSymbol, line, charPositionInLine,
                          msg, e);
  }
}

void ProxyErrorListener::reportAmbiguity(Parser* recognizer,
                                         const dfa::DFA& dfa, size_t startIndex,
                                         size_t stopIndex, bool exact,
                                         const antlrcpp::BitSet& ambigAlts,
                                         atn::ATNConfigSet* configs) {
  for (auto listener : _delegates) {
    listener->reportAmbiguity(recognizer, dfa, startIndex, stopIndex, exact,
                              ambigAlts, configs);
  }
}

void ProxyErrorListener::reportAttemptingFullContext(
    Parser* recognizer, const dfa::DFA& dfa, size_t startIndex,
    size_t stopIndex, const antlrcpp::BitSet& conflictingAlts,
    atn::ATNConfigSet* configs) {
  for (auto listener : _delegates) {
    listener->reportAttemptingFullContext(recognizer, dfa, startIndex,
                                          stopIndex, conflictingAlts, configs);
  }
}

void ProxyErrorListener::reportContextSensitivity(
    Parser* recognizer, const dfa::DFA& dfa, size_t startIndex,
    size_t stopIndex, size_t prediction, atn::ATNConfigSet* configs) {
  for (auto listener : _delegates) {
    listener->reportContextSensitivity(recognizer, dfa, startIndex, stopIndex,
                                       prediction, configs);
  }
}
