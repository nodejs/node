/* Copyright (c) 2012-2017 The ANTLR Project. All rights reserved.
 * Use of this file is governed by the BSD 3-clause license that
 * can be found in the LICENSE.txt file in the project root.
 */

#pragma once

#include "Lexer.h"
#include "Vocabulary.h"
#include "atn/PredictionContext.h"

namespace antlr4 {

class ANTLR4CPP_PUBLIC LexerInterpreter : public Lexer {
 public:
  // @deprecated
  LexerInterpreter(const std::string& grammarFileName,
                   const std::vector<std::string>& tokenNames,
                   const std::vector<std::string>& ruleNames,
                   const std::vector<std::string>& channelNames,
                   const std::vector<std::string>& modeNames,
                   const atn::ATN& atn, CharStream* input);
  LexerInterpreter(const std::string& grammarFileName,
                   const dfa::Vocabulary& vocabulary,
                   const std::vector<std::string>& ruleNames,
                   const std::vector<std::string>& channelNames,
                   const std::vector<std::string>& modeNames,
                   const atn::ATN& atn, CharStream* input);

  ~LexerInterpreter();

  virtual const atn::ATN& getATN() const override;
  virtual std::string getGrammarFileName() const override;
  virtual const std::vector<std::string>& getTokenNames() const override;
  virtual const std::vector<std::string>& getRuleNames() const override;
  virtual const std::vector<std::string>& getChannelNames() const override;
  virtual const std::vector<std::string>& getModeNames() const override;

  virtual const dfa::Vocabulary& getVocabulary() const override;

 protected:
  const std::string _grammarFileName;
  const atn::ATN& _atn;

  // @deprecated
  std::vector<std::string> _tokenNames;
  const std::vector<std::string>& _ruleNames;
  const std::vector<std::string>& _channelNames;
  const std::vector<std::string>& _modeNames;
  std::vector<dfa::DFA> _decisionToDFA;

  atn::PredictionContextCache _sharedContextCache;

 private:
  dfa::Vocabulary _vocabulary;
};

}  // namespace antlr4
