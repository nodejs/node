/* Copyright (c) 2012-2017 The ANTLR Project. All rights reserved.
 * Use of this file is governed by the BSD 3-clause license that
 * can be found in the LICENSE.txt file in the project root.
 */

#include "tree/ParseTree.h"
#include "tree/pattern/ParseTreeMatch.h"
#include "tree/pattern/ParseTreePatternMatcher.h"

#include "tree/xpath/XPath.h"
#include "tree/xpath/XPathElement.h"

#include "tree/pattern/ParseTreePattern.h"

using namespace antlr4::tree;
using namespace antlr4::tree::pattern;

using namespace antlrcpp;

ParseTreePattern::ParseTreePattern(ParseTreePatternMatcher* matcher,
                                   const std::string& pattern,
                                   int patternRuleIndex_,
                                   ParseTree* patternTree)
    : patternRuleIndex(patternRuleIndex_),
      _pattern(pattern),
      _patternTree(patternTree),
      _matcher(matcher) {}

ParseTreePattern::~ParseTreePattern() {}

ParseTreeMatch ParseTreePattern::match(ParseTree* tree) {
  return _matcher->match(tree, *this);
}

bool ParseTreePattern::matches(ParseTree* tree) {
  return _matcher->match(tree, *this).succeeded();
}

std::vector<ParseTreeMatch> ParseTreePattern::findAll(
    ParseTree* tree, const std::string& xpath) {
  xpath::XPath finder(_matcher->getParser(), xpath);
  std::vector<ParseTree*> subtrees = finder.evaluate(tree);
  std::vector<ParseTreeMatch> matches;
  for (auto t : subtrees) {
    ParseTreeMatch aMatch = match(t);
    if (aMatch.succeeded()) {
      matches.push_back(aMatch);
    }
  }
  return matches;
}

ParseTreePatternMatcher* ParseTreePattern::getMatcher() const {
  return _matcher;
}

std::string ParseTreePattern::getPattern() const { return _pattern; }

int ParseTreePattern::getPatternRuleIndex() const { return patternRuleIndex; }

ParseTree* ParseTreePattern::getPatternTree() const { return _patternTree; }
