'use strict';
const { isCommonModule } = require('./rules-utils.js');

function isAsyncIIFE(node) {
  const { callee: { type, async } } = node;
  const types = ['FunctionExpression', 'ArrowFunctionExpression'];
  return types.includes(type) && async;
}

const message =
  'The result of an immediately-invoked async function needs to be used ' +
  '(e.g. with `.then(common.mustCall())`)';

module.exports = {
  meta: {
    fixable: 'code',
  },
  create: function(context) {
    let hasCommonModule = false;
    return {
      CallExpression: function(node) {
        if (isCommonModule(node) && node.parent.type === 'VariableDeclarator') {
          hasCommonModule = true;
        }

        if (!isAsyncIIFE(node)) return;
        if (node.parent && node.parent.type === 'ExpressionStatement') {
          context.report({
            node,
            message,
            fix: (fixer) => {
              if (hasCommonModule)
                return fixer.insertTextAfter(node, '.then(common.mustCall())');
            },
          });
        }
      },
    };
  },
};
