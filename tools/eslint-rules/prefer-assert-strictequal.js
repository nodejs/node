'use strict';

function isAssert(node) {
  return node.expression &&
    node.expression.type === 'CallExpression' &&
    node.expression.callee.name === 'assert';
}

function getFirstArg(expression) {
  return expression.arguments && expression.arguments[0];
}

const strictEqualError = '`assert.strictEqual` should be used instead if `!==`';

module.exports = function(context) {
  return {
    ExpressionStatement(node) {
      try {
        if (isAssert(node)) {
          const arg = getFirstArg(node.expression);
          if (arg) {
            if (arg.type === 'BinaryExpression' && arg.operator === '!==') {
              context.report(node, strictEqualError);
            }
          }
        }
      } catch (e) {
        context.report(node, `${e.toString()} ${e.stack}`);
      }
    }
  };
};
