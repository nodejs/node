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
    CONST = 44,
    EXTERN = 45,
    ASSERT_TOKEN = 46,
    CHECK_TOKEN = 47,
    UNREACHABLE_TOKEN = 48,
    DEBUG_TOKEN = 49,
    ASSIGNMENT = 50,
    ASSIGNMENT_OPERATOR = 51,
    EQUAL = 52,
    PLUS = 53,
    MINUS = 54,
    MULTIPLY = 55,
    DIVIDE = 56,
    MODULO = 57,
    BIT_OR = 58,
    BIT_AND = 59,
    BIT_NOT = 60,
    MAX = 61,
    MIN = 62,
    NOT_EQUAL = 63,
    LESS_THAN = 64,
    LESS_THAN_EQUAL = 65,
    GREATER_THAN = 66,
    GREATER_THAN_EQUAL = 67,
    SHIFT_LEFT = 68,
    SHIFT_RIGHT = 69,
    SHIFT_RIGHT_ARITHMETIC = 70,
    VARARGS = 71,
    EQUALITY_OPERATOR = 72,
    INCREMENT = 73,
    DECREMENT = 74,
    NOT = 75,
    STRING_LITERAL = 76,
    IDENTIFIER = 77,
    WS = 78,
    BLOCK_COMMENT = 79,
    LINE_COMMENT = 80,
    DECIMAL_LITERAL = 81
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
