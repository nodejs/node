/**
 * @fileoverview Require use of new Buffer constructor methods in lib
 * @author James M Snell
 */
'use strict';

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------
const msg = 'Use of the Buffer() constructor has been deprecated. ' +
            'Please use either Buffer.alloc(), Buffer.allocUnsafe(), ' +
            'or Buffer.from()';

function test(context, node) {
  if (node.callee.name === 'Buffer') {
    const sourceCode = context.getSourceCode();
    const argumentList = [];
    node.arguments.forEach((argumentNode) => {
      argumentList.push(sourceCode.getText(argumentNode));
    });

    context.report({
      node,
      message: msg,
      fix: (fixer) => {
        return fixer.replaceText(
          node,
          `Buffer.from(${argumentList.join(', ')})`
        );
      }
    });
  }
}

module.exports = function(context) {
  return {
    'NewExpression': (node) => test(context, node),
    'CallExpression': (node) => test(context, node)
  };
};
