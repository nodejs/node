/**
 * @fileoverview Prohibit the use of assert operators ( ===, !==, ==, != )
 */

'use strict';

const astSelector = 'ExpressionStatement[expression.type="CallExpression"]' +
                    '[expression.callee.name="assert"]' +
                    '[expression.arguments.0.type="BinaryExpression"]';

function parseError(method, op) {
  return `'assert.${method}' should be used instead of '${op}'`;
}

function checkExpression(arg) {
  const type = arg.type;
  return arg && (type === 'BinaryExpression');
}

function preferedAssertMethod(op) {
  switch (op) {
    case '===': return 'strictEqual';
    case '!==': return 'notStrictEqual';
    case '==': return 'equal';
    case '!=': return 'notEqual';
  }
}

module.exports = function(context) {
  return {
    ExpressionStatement(node) {
      if (isAssert(node)) {
        const arg = getFirstArg(node.expression);
        if (checkExpression(arg)) {
          const assertMethod = preferedAssertMethod(arg.operator);
          if (assertMethod) {
            context.report({
              node,
              message: parseError(assertMethod, arg.operator),
              fix: (fixer) => {
                const sourceCode = context.getSourceCode();
                const args = `${sourceCode.getText(arg)}`;
                return fixer.replaceText(
                  node,
                  `assert.${assertMethod}(${args});`
                );
              }
            });
          }
        }
      }
    }
  };
};
