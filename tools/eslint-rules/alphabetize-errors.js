'use strict';

const { isDefiningError } = require('./rules-utils.js');

const prefix = 'Out of ASCIIbetical order - ';
const opStr = ' >= ';

function errorForNode(node) {
  return node.expression.arguments[0].value;
}

module.exports = {
  create: function(context) {
    let previousNode;
    return {
      ExpressionStatement: function(node) {
        if (!isDefiningError(node)) return;
        if (!previousNode) {
          previousNode = node;
          return;
        }
        const prev = errorForNode(previousNode);
        const curr = errorForNode(node);
        previousNode = node;
        if (prev >= curr) {
          const message = [prefix, prev, opStr, curr].join('');
          context.report({ node, message });
        }
      }
    };
  }
};
