// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef V8_TORQUE_TORQUELEXER_H_
#define V8_TORQUE_TORQUELEXER_H_

// Generated from Torque.g4 by ANTLR 4.7.1

#pragma once

#include "./antlr4-runtime.h"

class TorqueLexer : public antlr4::Lexer {
 public:
  enum {
    T__0 = 1,
    T__1 = 2,
    T__2 = 3,
    T__3 = 4,
    T__4 = 5,
    T__5 = 6,
    T__6 = 7,
    T__7 = 8,
    T__8 = 9,
    T__9 = 10,
    T__10 = 11,
    T__11 = 12,
    T__12 = 13,
    T__13 = 14,
    T__14 = 15,
    T__15 = 16,
    T__16 = 17,
    T__17 = 18,
    T__18 = 19,
    T__19 = 20,
    T__20 = 21,
    MACRO = 22,
    BUILTIN = 23,
    RUNTIME = 24,
    MODULE = 25,
    JAVASCRIPT = 26,
    DEFERRED = 27,
    IF = 28,
    FOR = 29,
    WHILE = 30,
    RETURN = 31,
    CONSTEXPR = 32,
    CONTINUE = 33,
    BREAK = 34,
    GOTO = 35,
    OTHERWISE = 36,
    TRY = 37,
    LABEL = 38,
    LABELS = 39,
    TAIL = 40,
    ISNT = 41,
    IS = 42,
    LET = 43,
    EXTERN = 44,
    ASSERT_TOKEN = 45,
    CHECK_TOKEN = 46,
    UNREACHABLE_TOKEN = 47,
    DEBUG_TOKEN = 48,
    ASSIGNMENT = 49,
    ASSIGNMENT_OPERATOR = 50,
    EQUAL = 51,
    PLUS = 52,
    MINUS = 53,
    MULTIPLY = 54,
    DIVIDE = 55,
    MODULO = 56,
    BIT_OR = 57,
    BIT_AND = 58,
    BIT_NOT = 59,
    MAX = 60,
    MIN = 61,
    NOT_EQUAL = 62,
    LESS_THAN = 63,
    LESS_THAN_EQUAL = 64,
    GREATER_THAN = 65,
    GREATER_THAN_EQUAL = 66,
    SHIFT_LEFT = 67,
    SHIFT_RIGHT = 68,
    SHIFT_RIGHT_ARITHMETIC = 69,
    VARARGS = 70,
    EQUALITY_OPERATOR = 71,
    INCREMENT = 72,
    DECREMENT = 73,
    NOT = 74,
    STRING_LITERAL = 75,
    IDENTIFIER = 76,
    WS = 77,
    BLOCK_COMMENT = 78,
    LINE_COMMENT = 79,
    DECIMAL_LITERAL = 80
  };

  explicit TorqueLexer(antlr4::CharStream* input);
  ~TorqueLexer();

  std::string getGrammarFileName() const override;
  const std::vector<std::string>& getRuleNames() const override;

  const std::vector<std::string>& getChannelNames() const override;
  const std::vector<std::string>& getModeNames() const override;
  const std::vector<std::string>& getTokenNames()
      const override;  // deprecated, use vocabulary instead
  antlr4::dfa::Vocabulary& getVocabulary() const override;

  const std::vector<uint16_t> getSerializedATN() const override;
  const antlr4::atn::ATN& getATN() const override;

 private:
  static std::vector<antlr4::dfa::DFA> _decisionToDFA;
  static antlr4::atn::PredictionContextCache _sharedContextCache;
  static std::vector<std::string> _ruleNames;
  static std::vector<std::string> _tokenNames;
  static std::vector<std::string> _channelNames;
  static std::vector<std::string> _modeNames;

  static std::vector<std::string> _literalNames;
  static std::vector<std::string> _symbolicNames;
  static antlr4::dfa::Vocabulary _vocabulary;
  static antlr4::atn::ATN _atn;
  static std::vector<uint16_t> _serializedATN;

  // Individual action functions triggered by action() above.

  // Individual semantic predicate functions triggered by sempred() above.

  struct Initializer {
    Initializer();
  };
  static Initializer _init;
};

#endif  // V8_TORQUE_TORQUELEXER_H_
