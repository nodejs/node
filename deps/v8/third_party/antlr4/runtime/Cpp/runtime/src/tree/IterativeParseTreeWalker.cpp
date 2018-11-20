/* Copyright (c) 2012-2017 The ANTLR Project. All rights reserved.
 * Use of this file is governed by the BSD 3-clause license that
 * can be found in the LICENSE.txt file in the project root.
 */

#include "support/CPPUtils.h"

#include "tree/ErrorNode.h"
#include "tree/ParseTree.h"
#include "tree/ParseTreeListener.h"

#include "IterativeParseTreeWalker.h"

using namespace antlr4::tree;

void IterativeParseTreeWalker::walk(ParseTreeListener* listener,
                                    ParseTree* t) const {
  std::vector<ParseTree*> nodeStack;
  std::vector<size_t> indexStack;

  ParseTree* currentNode = t;
  size_t currentIndex = 0;

  while (currentNode != nullptr) {
    // pre-order visit
    if (antlrcpp::is<ErrorNode*>(currentNode)) {
      listener->visitErrorNode(dynamic_cast<ErrorNode*>(currentNode));
    } else if (antlrcpp::is<TerminalNode*>(currentNode)) {
      listener->visitTerminal((TerminalNode*)currentNode);
    } else {
      enterRule(listener, currentNode);
    }

    // Move down to first child, if it exists.
    if (!currentNode->children.empty()) {
      nodeStack.push_back(currentNode);
      indexStack.push_back(currentIndex);
      currentIndex = 0;
      currentNode = currentNode->children[0];
      continue;
    }

    // No child nodes, so walk tree.
    do {
      // post-order visit
      if (!antlrcpp::is<TerminalNode*>(currentNode)) {
        exitRule(listener, currentNode);
      }

      // No parent, so no siblings.
      if (nodeStack.empty()) {
        currentNode = nullptr;
        currentIndex = 0;
        break;
      }

      // Move to next sibling if possible.
      if (nodeStack.back()->children.size() > ++currentIndex) {
        currentNode = nodeStack.back()->children[currentIndex];
        break;
      }

      // No next sibling, so move up.
      currentNode = nodeStack.back();
      nodeStack.pop_back();
      currentIndex = indexStack.back();
      indexStack.pop_back();

    } while (currentNode != nullptr);
  }
}
