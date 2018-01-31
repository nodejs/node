'use strict';

const astSelector = 'ExpressionStatement[expression.type="CallExpression"]' +
                    '[expression.callee.name="assert"]' +
                    '[expression.arguments.0.type="BinaryExpression"]';

function parseError(method, op) {
  return `'assert.${method}' should be used instead of '${op}'`;
}

const preferedAssertMethod = {
  '===': 'strictEqual',
  '!==': 'notStrictEqual',
  '==': 'equal',
  '!=': 'notEqual'
};

module.exports = function(context) {
  return {
    [astSelector]: function(node) {
      const arg = node.expression.arguments[0];
      const assertMethod = preferedAssertMethod[arg.operator];
      if (assertMethod) {
        context.report(node, parseError(assertMethod, arg.operator));
      }
    }
  };
};
