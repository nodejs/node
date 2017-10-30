'use strict';

const prefix = 'Out of ASCIIbetical order - ';
const opStr = ' >= ';

function errorForNode(node) {
  return node.expression.arguments[0].value;
}

function isDefiningError(node) {
  return node.expression &&
         node.expression.type === 'CallExpression' &&
         node.expression.callee &&
         node.expression.callee.name === 'E';
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
