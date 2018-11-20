/* Copyright (c) 2012-2017 The ANTLR Project. All rights reserved.
 * Use of this file is governed by the BSD 3-clause license that
 * can be found in the LICENSE.txt file in the project root.
 */

#include "CommonToken.h"
#include "Parser.h"
#include "ParserRuleContext.h"
#include "Token.h"
#include "atn/ATN.h"
#include "misc/Interval.h"
#include "misc/Predicate.h"
#include "support/CPPUtils.h"
#include "tree/ErrorNode.h"
#include "tree/TerminalNodeImpl.h"

#include "tree/Trees.h"

using namespace antlr4;
using namespace antlr4::misc;
using namespace antlr4::tree;

using namespace antlrcpp;

Trees::Trees() {}

std::string Trees::toStringTree(ParseTree* t) {
  return toStringTree(t, nullptr);
}

std::string Trees::toStringTree(ParseTree* t, Parser* recog) {
  if (recog == nullptr) return toStringTree(t, std::vector<std::string>());
  return toStringTree(t, recog->getRuleNames());
}

std::string Trees::toStringTree(ParseTree* t,
                                const std::vector<std::string>& ruleNames) {
  std::string temp =
      antlrcpp::escapeWhitespace(Trees::getNodeText(t, ruleNames), false);
  if (t->children.empty()) {
    return temp;
  }

  std::stringstream ss;
  ss << "(" << temp << ' ';

  // Implement the recursive walk as iteration to avoid trouble with deep
  // nesting.
  std::stack<size_t> stack;
  size_t childIndex = 0;
  ParseTree* run = t;
  while (childIndex < run->children.size()) {
    if (childIndex > 0) {
      ss << ' ';
    }
    ParseTree* child = run->children[childIndex];
    temp =
        antlrcpp::escapeWhitespace(Trees::getNodeText(child, ruleNames), false);
    if (!child->children.empty()) {
      // Go deeper one level.
      stack.push(childIndex);
      run = child;
      childIndex = 0;
      ss << "(" << temp << " ";
    } else {
      ss << temp;
      while (++childIndex == run->children.size()) {
        if (stack.size() > 0) {
          // Reached the end of the current level. See if we can step up from
          // here.
          childIndex = stack.top();
          stack.pop();
          run = run->parent;
          ss << ")";
        } else {
          break;
        }
      }
    }
  }

  ss << ")";
  return ss.str();
}

std::string Trees::getNodeText(ParseTree* t, Parser* recog) {
  return getNodeText(t, recog->getRuleNames());
}

std::string Trees::getNodeText(ParseTree* t,
                               const std::vector<std::string>& ruleNames) {
  if (ruleNames.size() > 0) {
    if (is<RuleContext*>(t)) {
      size_t ruleIndex = dynamic_cast<RuleContext*>(t)->getRuleIndex();
      std::string ruleName = ruleNames[ruleIndex];
      size_t altNumber = dynamic_cast<RuleContext*>(t)->getAltNumber();
      if (altNumber != atn::ATN::INVALID_ALT_NUMBER) {
        return ruleName + ":" + std::to_string(altNumber);
      }
      return ruleName;
    } else if (is<ErrorNode*>(t)) {
      return t->toString();
    } else if (is<TerminalNode*>(t)) {
      Token* symbol = dynamic_cast<TerminalNode*>(t)->getSymbol();
      if (symbol != nullptr) {
        std::string s = symbol->getText();
        return s;
      }
    }
  }
  // no recog for rule names
  if (is<RuleContext*>(t)) {
    return dynamic_cast<RuleContext*>(t)->getText();
  }

  if (is<TerminalNodeImpl*>(t)) {
    return dynamic_cast<TerminalNodeImpl*>(t)->getSymbol()->getText();
  }

  return "";
}

std::vector<ParseTree*> Trees::getAncestors(ParseTree* t) {
  std::vector<ParseTree*> ancestors;
  ParseTree* parent = t->parent;
  while (parent != nullptr) {
    ancestors.insert(ancestors.begin(), parent);  // insert at start
    parent = parent->parent;
  }
  return ancestors;
}

template <typename T>
static void _findAllNodes(ParseTree* t, size_t index, bool findTokens,
                          std::vector<T>& nodes) {
  // check this node (the root) first
  if (findTokens && is<TerminalNode*>(t)) {
    TerminalNode* tnode = dynamic_cast<TerminalNode*>(t);
    if (tnode->getSymbol()->getType() == index) {
      nodes.push_back(t);
    }
  } else if (!findTokens && is<ParserRuleContext*>(t)) {
    ParserRuleContext* ctx = dynamic_cast<ParserRuleContext*>(t);
    if (ctx->getRuleIndex() == index) {
      nodes.push_back(t);
    }
  }
  // check children
  for (size_t i = 0; i < t->children.size(); i++) {
    _findAllNodes(t->children[i], index, findTokens, nodes);
  }
}

bool Trees::isAncestorOf(ParseTree* t, ParseTree* u) {
  if (t == nullptr || u == nullptr || t->parent == nullptr) {
    return false;
  }

  ParseTree* p = u->parent;
  while (p != nullptr) {
    if (t == p) {
      return true;
    }
    p = p->parent;
  }
  return false;
}

std::vector<ParseTree*> Trees::findAllTokenNodes(ParseTree* t, size_t ttype) {
  return findAllNodes(t, ttype, true);
}

std::vector<ParseTree*> Trees::findAllRuleNodes(ParseTree* t,
                                                size_t ruleIndex) {
  return findAllNodes(t, ruleIndex, false);
}

std::vector<ParseTree*> Trees::findAllNodes(ParseTree* t, size_t index,
                                            bool findTokens) {
  std::vector<ParseTree*> nodes;
  _findAllNodes<ParseTree*>(t, index, findTokens, nodes);
  return nodes;
}

std::vector<ParseTree*> Trees::getDescendants(ParseTree* t) {
  std::vector<ParseTree*> nodes;
  nodes.push_back(t);
  std::size_t n = t->children.size();
  for (size_t i = 0; i < n; i++) {
    auto descentants = getDescendants(t->children[i]);
    for (auto entry : descentants) {
      nodes.push_back(entry);
    }
  }
  return nodes;
}

std::vector<ParseTree*> Trees::descendants(ParseTree* t) {
  return getDescendants(t);
}

ParserRuleContext* Trees::getRootOfSubtreeEnclosingRegion(
    ParseTree* t, size_t startTokenIndex, size_t stopTokenIndex) {
  size_t n = t->children.size();
  for (size_t i = 0; i < n; i++) {
    ParserRuleContext* r = getRootOfSubtreeEnclosingRegion(
        t->children[i], startTokenIndex, stopTokenIndex);
    if (r != nullptr) {
      return r;
    }
  }

  if (is<ParserRuleContext*>(t)) {
    ParserRuleContext* r = dynamic_cast<ParserRuleContext*>(t);
    if (startTokenIndex >=
            r->getStart()->getTokenIndex() &&  // is range fully contained in t?
        (r->getStop() == nullptr ||
         stopTokenIndex <= r->getStop()->getTokenIndex())) {
      // note: r.getStop()==null likely implies that we bailed out of parser and
      // there's nothing to the right
      return r;
    }
  }
  return nullptr;
}

ParseTree* Trees::findNodeSuchThat(ParseTree* t, Ref<Predicate> const& pred) {
  if (pred->test(t)) {
    return t;
  }

  size_t n = t->children.size();
  for (size_t i = 0; i < n; ++i) {
    ParseTree* u = findNodeSuchThat(t->children[i], pred);
    if (u != nullptr) {
      return u;
    }
  }

  return nullptr;
}
