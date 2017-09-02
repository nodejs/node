'use strict';

const message = 'Errors in lib/internal/errors.js must be alphabetized';

function errorForNode(node) {
  return node.expression.arguments[0].value;
}

function isAlphabetized(previousNode, node) {
  return errorForNode(previousNode).localeCompare(errorForNode(node)) < 0;
}

function isDefiningError(node) {
  return node.expression &&
         node.expression.type === 'CallExpression' &&
         node.expression.callee &&
         node.expression.callee.name === 'E';
}

module.exports = {
  create: function(context) {
    var previousNode;

    return {
      ExpressionStatement: function(node) {
        if (isDefiningError(node)) {
          if (previousNode && !isAlphabetized(previousNode, node)) {
            context.report({ node: node, message: message });
          }

          previousNode = node;
        }
      }
    };
  }
};
