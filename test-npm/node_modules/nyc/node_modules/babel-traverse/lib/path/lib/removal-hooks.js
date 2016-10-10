"use strict";

exports.__esModule = true;
// this file contains hooks that handle ancestry cleanup of parent nodes when removing children

/**
 * Pre hooks should be used for either rejecting removal or delegating removal
 */

var hooks = exports.hooks = [function (self, parent) {
  if (self.key === "body" && parent.isArrowFunctionExpression()) {
    self.replaceWith(self.scope.buildUndefinedNode());
    return true;
  }
}, function (self, parent) {
  var removeParent = false;

  // while (NODE);
  // removing the test of a while/switch, we can either just remove it entirely *or* turn the `test` into `true`
  // unlikely that the latter will ever be what's wanted so we just remove the loop to avoid infinite recursion
  removeParent = removeParent || self.key === "test" && (parent.isWhile() || parent.isSwitchCase());

  // export NODE;
  // just remove a declaration for an export as this is no longer valid
  removeParent = removeParent || self.key === "declaration" && parent.isExportDeclaration();

  // label: NODE
  // stray labeled statement with no body
  removeParent = removeParent || self.key === "body" && parent.isLabeledStatement();

  // let NODE;
  // remove an entire declaration if there are no declarators left
  removeParent = removeParent || self.listKey === "declarations" && parent.isVariableDeclaration() && parent.node.declarations.length === 1;

  // NODE;
  // remove the entire expression statement if there's no expression
  removeParent = removeParent || self.key === "expression" && parent.isExpressionStatement();

  if (removeParent) {
    parent.remove();
    return true;
  }
}, function (self, parent) {
  if (parent.isSequenceExpression() && parent.node.expressions.length === 1) {
    // (node, NODE);
    // we've just removed the second element of a sequence expression so let's turn that sequence
    // expression into a regular expression
    parent.replaceWith(parent.node.expressions[0]);
    return true;
  }
}, function (self, parent) {
  if (parent.isBinary()) {
    // left + NODE;
    // NODE + right;
    // we're in a binary expression, better remove it and replace it with the last expression
    if (self.key === "left") {
      parent.replaceWith(parent.node.right);
    } else {
      // key === "right"
      parent.replaceWith(parent.node.left);
    }
    return true;
  }
}, function (self, parent) {
  if (parent.isIfStatement() && (self.key === 'consequent' || self.key === 'alternate') || parent.isLoop() && self.key === 'body') {
    self.replaceWith({
      type: 'BlockStatement',
      body: []
    });
    return true;
  }
}];