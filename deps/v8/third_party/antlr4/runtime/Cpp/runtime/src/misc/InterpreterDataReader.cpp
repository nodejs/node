/* Copyright (c) 2012-2017 The ANTLR Project. All rights reserved.
 * Use of this file is governed by the BSD 3-clause license that
 * can be found in the LICENSE.txt file in the project root.
 */

#include "Vocabulary.h"
#include "atn/ATN.h"
#include "atn/ATNDeserializer.h"

#include "misc/InterpreterDataReader.h"

using namespace antlr4::dfa;
using namespace antlr4::atn;
using namespace antlr4::misc;

InterpreterData::InterpreterData(std::vector<std::string> const& literalNames,
                                 std::vector<std::string> const& symbolicNames)
    : vocabulary(literalNames, symbolicNames) {}

InterpreterData InterpreterDataReader::parseFile(std::string const& fileName) {
  // The structure of the data file is very simple. Everything is line based
  // with empty lines separating the different parts. For lexers the layout is:
  // token literal names:
  // ...
  //
  // token symbolic names:
  // ...
  //
  // rule names:
  // ...
  //
  // channel names:
  // ...
  //
  // mode names:
  // ...
  //
  // atn:
  // <a single line with comma separated int values> enclosed in a pair of
  // squared brackets.
  //
  // Data for a parser does not contain channel and mode names.

  std::ifstream input(fileName);
  if (!input.good()) return {};

  std::vector<std::string> literalNames;
  std::vector<std::string> symbolicNames;

  std::string line;

  std::getline(input, line, '\n');
  assert(line == "token literal names:");
  while (true) {
    std::getline(input, line, '\n');
    if (line.empty()) break;

    literalNames.push_back(line == "null" ? "" : line);
  };

  std::getline(input, line, '\n');
  assert(line == "token symbolic names:");
  while (true) {
    std::getline(input, line, '\n');
    if (line.empty()) break;

    symbolicNames.push_back(line == "null" ? "" : line);
  };
  InterpreterData result(literalNames, symbolicNames);

  std::getline(input, line, '\n');
  assert(line == "rule names:");
  while (true) {
    std::getline(input, line, '\n');
    if (line.empty()) break;

    result.ruleNames.push_back(line);
  };

  std::getline(input, line, '\n');
  if (line == "channel names:") {
    while (true) {
      std::getline(input, line, '\n');
      if (line.empty()) break;

      result.channels.push_back(line);
    };

    std::getline(input, line, '\n');
    assert(line == "mode names:");
    while (true) {
      std::getline(input, line, '\n');
      if (line.empty()) break;

      result.modes.push_back(line);
    };
  }

  std::vector<uint16_t> serializedATN;

  std::getline(input, line, '\n');
  assert(line == "atn:");
  std::getline(input, line, '\n');
  std::stringstream tokenizer(line);
  std::string value;
  while (tokenizer.good()) {
    std::getline(tokenizer, value, ',');
    unsigned long number;
    if (value[0] == '[')
      number = std::strtoul(&value[1], nullptr, 10);
    else
      number = std::strtoul(value.c_str(), nullptr, 10);
    serializedATN.push_back(static_cast<uint16_t>(number));
  }

  ATNDeserializer deserializer;
  result.atn = deserializer.deserialize(serializedATN);
  return result;
}
