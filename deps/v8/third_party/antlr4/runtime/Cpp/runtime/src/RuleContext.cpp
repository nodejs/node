/* Copyright (c) 2012-2017 The ANTLR Project. All rights reserved.
 * Use of this file is governed by the BSD 3-clause license that
 * can be found in the LICENSE.txt file in the project root.
 */

#include "Parser.h"
#include "atn/ATN.h"
#include "atn/ATNState.h"
#include "misc/Interval.h"
#include "tree/ParseTreeVisitor.h"
#include "tree/Trees.h"

#include "RuleContext.h"

using namespace antlr4;
using namespace antlr4::atn;

RuleContext::RuleContext() { InitializeInstanceFields(); }

RuleContext::RuleContext(RuleContext* parent_, size_t invokingState_) {
  InitializeInstanceFields();
  this->parent = parent_;
  this->invokingState = invokingState_;
}

int RuleContext::depth() {
  int n = 1;
  RuleContext* p = this;
  while (true) {
    if (p->parent == nullptr) break;
    p = static_cast<RuleContext*>(p->parent);
    n++;
  }
  return n;
}

bool RuleContext::isEmpty() {
  return invokingState == ATNState::INVALID_STATE_NUMBER;
}

misc::Interval RuleContext::getSourceInterval() {
  return misc::Interval::INVALID;
}

std::string RuleContext::getText() {
  if (children.empty()) {
    return "";
  }

  std::stringstream ss;
  for (size_t i = 0; i < children.size(); i++) {
    ParseTree* tree = children[i];
    if (tree != nullptr) ss << tree->getText();
  }

  return ss.str();
}

size_t RuleContext::getRuleIndex() const { return INVALID_INDEX; }

size_t RuleContext::getAltNumber() const {
  return atn::ATN::INVALID_ALT_NUMBER;
}

void RuleContext::setAltNumber(size_t /*altNumber*/) {}

antlrcpp::Any RuleContext::accept(tree::ParseTreeVisitor* visitor) {
  return visitor->visitChildren(this);
}

std::string RuleContext::toStringTree(Parser* recog) {
  return tree::Trees::toStringTree(this, recog);
}

std::string RuleContext::toStringTree(std::vector<std::string>& ruleNames) {
  return tree::Trees::toStringTree(this, ruleNames);
}

std::string RuleContext::toStringTree() { return toStringTree(nullptr); }

std::string RuleContext::toString(const std::vector<std::string>& ruleNames) {
  return toString(ruleNames, nullptr);
}

std::string RuleContext::toString(const std::vector<std::string>& ruleNames,
                                  RuleContext* stop) {
  std::stringstream ss;

  RuleContext* currentParent = this;
  ss << "[";
  while (currentParent != stop) {
    if (ruleNames.empty()) {
      if (!currentParent->isEmpty()) {
        ss << currentParent->invokingState;
      }
    } else {
      size_t ruleIndex = currentParent->getRuleIndex();

      std::string ruleName = (ruleIndex < ruleNames.size())
                                 ? ruleNames[ruleIndex]
                                 : std::to_string(ruleIndex);
      ss << ruleName;
    }

    if (currentParent->parent == nullptr)  // No parent anymore.
      break;
    currentParent = static_cast<RuleContext*>(currentParent->parent);
    if (!ruleNames.empty() || !currentParent->isEmpty()) {
      ss << " ";
    }
  }

  ss << "]";

  return ss.str();
}

std::string RuleContext::toString() { return toString(nullptr); }

std::string RuleContext::toString(Recognizer* recog) {
  return toString(recog, &ParserRuleContext::EMPTY);
}

std::string RuleContext::toString(Recognizer* recog, RuleContext* stop) {
  if (recog == nullptr)
    return toString(std::vector<std::string>(), stop);  // Don't use an
                                                        // initializer {} here
                                                        // or we end up calling
                                                        // ourselve recursivly.
  return toString(recog->getRuleNames(), stop);
}

void RuleContext::InitializeInstanceFields() { invokingState = INVALID_INDEX; }
