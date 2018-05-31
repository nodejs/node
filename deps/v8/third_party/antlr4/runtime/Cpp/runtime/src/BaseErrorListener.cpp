/* Copyright (c) 2012-2017 The ANTLR Project. All rights reserved.
 * Use of this file is governed by the BSD 3-clause license that
 * can be found in the LICENSE.txt file in the project root.
 */

#include "BaseErrorListener.h"
#include "RecognitionException.h"

using namespace antlr4;

void BaseErrorListener::syntaxError(Recognizer* /*recognizer*/,
                                    Token* /*offendingSymbol*/, size_t /*line*/,
                                    size_t /*charPositionInLine*/,
                                    const std::string& /*msg*/,
                                    std::exception_ptr /*e*/) {}

void BaseErrorListener::reportAmbiguity(Parser* /*recognizer*/,
                                        const dfa::DFA& /*dfa*/,
                                        size_t /*startIndex*/,
                                        size_t /*stopIndex*/, bool /*exact*/,
                                        const antlrcpp::BitSet& /*ambigAlts*/,
                                        atn::ATNConfigSet* /*configs*/) {}

void BaseErrorListener::reportAttemptingFullContext(
    Parser* /*recognizer*/, const dfa::DFA& /*dfa*/, size_t /*startIndex*/,
    size_t /*stopIndex*/, const antlrcpp::BitSet& /*conflictingAlts*/,
    atn::ATNConfigSet* /*configs*/) {}

void BaseErrorListener::reportContextSensitivity(
    Parser* /*recognizer*/, const dfa::DFA& /*dfa*/, size_t /*startIndex*/,
    size_t /*stopIndex*/, size_t /*prediction*/,
    atn::ATNConfigSet* /*configs*/) {}
