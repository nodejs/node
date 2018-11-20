/* Copyright (c) 2012-2017 The ANTLR Project. All rights reserved.
 * Use of this file is governed by the BSD 3-clause license that
 * can be found in the LICENSE.txt file in the project root.
 */

#pragma once

#include "antlr4-common.h"

namespace antlr4 {
namespace misc {

struct InterpreterData {
  atn::ATN atn;
  dfa::Vocabulary vocabulary;
  std::vector<std::string> ruleNames;
  std::vector<std::string> channels;  // Only valid for lexer grammars.
  std::vector<std::string> modes;     // ditto

  InterpreterData(){};  // For invalid content.
  InterpreterData(std::vector<std::string> const& literalNames,
                  std::vector<std::string> const& symbolicNames);
};

// A class to read plain text interpreter data produced by ANTLR.
class ANTLR4CPP_PUBLIC InterpreterDataReader {
 public:
  static InterpreterData parseFile(std::string const& fileName);
};

}  // namespace misc
}  // namespace antlr4
