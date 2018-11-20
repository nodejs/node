/* Copyright (c) 2012-2017 The ANTLR Project. All rights reserved.
 * Use of this file is governed by the BSD 3-clause license that
 * can be found in the LICENSE.txt file in the project root.
 */

#include "ParserRuleContext.h"
#include "support/CPPUtils.h"
#include "tree/ErrorNode.h"
#include "tree/ParseTreeListener.h"

#include "tree/IterativeParseTreeWalker.h"
#include "tree/ParseTreeWalker.h"

using namespace antlr4::tree;
using namespace antlrcpp;

static IterativeParseTreeWalker defaultWalker;
ParseTreeWalker& ParseTreeWalker::DEFAULT = defaultWalker;

ParseTreeWalker::~ParseTreeWalker() {}

void ParseTreeWalker::walk(ParseTreeListener* listener, ParseTree* t) const {
  if (is<ErrorNode*>(t)) {
    listener->visitErrorNode(dynamic_cast<ErrorNode*>(t));
    return;
  } else if (is<TerminalNode*>(t)) {
    listener->visitTerminal(dynamic_cast<TerminalNode*>(t));
    return;
  }

  enterRule(listener, t);
  for (auto& child : t->children) {
    walk(listener, child);
  }
  exitRule(listener, t);
}

void ParseTreeWalker::enterRule(ParseTreeListener* listener,
                                ParseTree* r) const {
  ParserRuleContext* ctx = dynamic_cast<ParserRuleContext*>(r);
  listener->enterEveryRule(ctx);
  ctx->enterRule(listener);
}

void ParseTreeWalker::exitRule(ParseTreeListener* listener,
                               ParseTree* r) const {
  ParserRuleContext* ctx = dynamic_cast<ParserRuleContext*>(r);
  ctx->exitRule(listener);
  listener->exitEveryRule(ctx);
}
