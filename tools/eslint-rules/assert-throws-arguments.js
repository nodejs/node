/**
 * @fileoverview Check that assert.throws is never called with a string as
 * second argument.
 * @author Michaël Zasso
 */
'use strict';

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

function checkThrowsArguments(context, node) {
  if (node.callee.type === 'MemberExpression' &&
      node.callee.object.name === 'assert' &&
      node.callee.property.name === 'throws') {
    const args = node.arguments;
    if (args.length > 3) {
      context.report({
        message: 'Too many arguments',
        node: node
      });
    } else if (args.length > 1) {
      const error = args[1];
      if (error.type === 'Literal' && typeof error.value === 'string') {
        context.report({
          message: 'Unexpected string as second argument',
          node: error
        });
      }
    } else {
      if (context.options[0].requireTwo) {
        context.report({
          message: 'Expected at least two arguments',
          node: node
        });
      }
    }
  }
}

module.exports = {
  meta: {
    schema: [
      {
        type: 'object',
        properties: {
          requireTwo: {
            type: 'boolean'
          }
        }
      }
    ]
  },
  create: function(context) {
    return {
      CallExpression: (node) => checkThrowsArguments(context, node)
    };
  }
};
