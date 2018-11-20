/* Copyright (c) 2012-2017 The ANTLR Project. All rights reserved.
 * Use of this file is governed by the BSD 3-clause license that
 * can be found in the LICENSE.txt file in the project root.
 */

#include "Exceptions.h"
#include "Vocabulary.h"
#include "atn/ATNType.h"
#include "atn/EmptyPredictionContext.h"
#include "atn/LexerATNSimulator.h"
#include "dfa/DFA.h"

#include "LexerInterpreter.h"

using namespace antlr4;

LexerInterpreter::LexerInterpreter(const std::string& grammarFileName,
                                   const std::vector<std::string>& tokenNames,
                                   const std::vector<std::string>& ruleNames,
                                   const std::vector<std::string>& channelNames,
                                   const std::vector<std::string>& modeNames,
                                   const atn::ATN& atn, CharStream* input)
    : LexerInterpreter(grammarFileName,
                       dfa::Vocabulary::fromTokenNames(tokenNames), ruleNames,
                       channelNames, modeNames, atn, input) {}

LexerInterpreter::LexerInterpreter(const std::string& grammarFileName,
                                   const dfa::Vocabulary& vocabulary,
                                   const std::vector<std::string>& ruleNames,
                                   const std::vector<std::string>& channelNames,
                                   const std::vector<std::string>& modeNames,
                                   const atn::ATN& atn, CharStream* input)
    : Lexer(input),
      _grammarFileName(grammarFileName),
      _atn(atn),
      _ruleNames(ruleNames),
      _channelNames(channelNames),
      _modeNames(modeNames),
      _vocabulary(vocabulary) {
  if (_atn.grammarType != atn::ATNType::LEXER) {
    throw IllegalArgumentException("The ATN must be a lexer ATN.");
  }

  for (size_t i = 0; i < atn.maxTokenType; i++) {
    _tokenNames.push_back(vocabulary.getDisplayName(i));
  }

  for (size_t i = 0; i < atn.getNumberOfDecisions(); ++i) {
    _decisionToDFA.push_back(dfa::DFA(_atn.getDecisionState(i), i));
  }
  _interpreter = new atn::LexerATNSimulator(
      this, _atn, _decisionToDFA,
      _sharedContextCache); /* mem-check: deleted in d-tor */
}

LexerInterpreter::~LexerInterpreter() { delete _interpreter; }

const atn::ATN& LexerInterpreter::getATN() const { return _atn; }

std::string LexerInterpreter::getGrammarFileName() const {
  return _grammarFileName;
}

const std::vector<std::string>& LexerInterpreter::getTokenNames() const {
  return _tokenNames;
}

const std::vector<std::string>& LexerInterpreter::getRuleNames() const {
  return _ruleNames;
}

const std::vector<std::string>& LexerInterpreter::getChannelNames() const {
  return _channelNames;
}

const std::vector<std::string>& LexerInterpreter::getModeNames() const {
  return _modeNames;
}

const dfa::Vocabulary& LexerInterpreter::getVocabulary() const {
  return _vocabulary;
}
