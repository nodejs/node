/* Copyright (c) 2012-2017 The ANTLR Project. All rights reserved.
 * Use of this file is governed by the BSD 3-clause license that
 * can be found in the LICENSE.txt file in the project root.
 */

#include "Exceptions.h"

#include "tree/pattern/ParseTreeMatch.h"

using namespace antlr4::tree;
using namespace antlr4::tree::pattern;

ParseTreeMatch::ParseTreeMatch(
    ParseTree* tree, const ParseTreePattern& pattern,
    const std::map<std::string, std::vector<ParseTree*>>& labels,
    ParseTree* mismatchedNode)
    : _tree(tree),
      _pattern(pattern),
      _labels(labels),
      _mismatchedNode(mismatchedNode) {
  if (tree == nullptr) {
    throw IllegalArgumentException("tree cannot be nul");
  }
}

ParseTreeMatch::~ParseTreeMatch() {}

ParseTree* ParseTreeMatch::get(const std::string& label) {
  auto iterator = _labels.find(label);
  if (iterator == _labels.end() || iterator->second.empty()) {
    return nullptr;
  }

  return iterator->second.back();  // return last if multiple
}

std::vector<ParseTree*> ParseTreeMatch::getAll(const std::string& label) {
  auto iterator = _labels.find(label);
  if (iterator == _labels.end()) {
    return {};
  }

  return iterator->second;
}

std::map<std::string, std::vector<ParseTree*>>& ParseTreeMatch::getLabels() {
  return _labels;
}

ParseTree* ParseTreeMatch::getMismatchedNode() { return _mismatchedNode; }

bool ParseTreeMatch::succeeded() { return _mismatchedNode == nullptr; }

const ParseTreePattern& ParseTreeMatch::getPattern() { return _pattern; }

ParseTree* ParseTreeMatch::getTree() { return _tree; }

std::string ParseTreeMatch::toString() {
  if (succeeded()) {
    return "Match succeeded; found " + std::to_string(_labels.size()) +
           " labels";
  } else {
    return "Match failed; found " + std::to_string(_labels.size()) + " labels";
  }
}
