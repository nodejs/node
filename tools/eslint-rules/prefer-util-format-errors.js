'use strict';

const errMsg = 'Please use a printf-like formatted string that util.format' +
               ' can consume.';

function isArrowFunctionWithTemplateLiteral(node) {
  return node.type === 'ArrowFunctionExpression' &&
         node.body.type === 'TemplateLiteral';
}

function isDefiningError(node) {
  return node.expression &&
         node.expression.type === 'CallExpression' &&
         node.expression.callee &&
         node.expression.callee.name === 'E';
}

module.exports = {
  create: function(context) {
    return {
      ExpressionStatement: function(node) {
        if (!isDefiningError(node))
          return;

        const msg = node.expression.arguments[1];
        if (!isArrowFunctionWithTemplateLiteral(msg))
          return;

        const { expressions } = msg.body;
        const hasSequentialParams = msg.params.every((param, index) => {
          const expr = expressions[index];
          return expr && expr.type === 'Identifier' && param.name === expr.name;
        });
        if (hasSequentialParams)
          context.report(msg, errMsg);
      }
    };
  }
};
