'use strict';

const { isDefiningError } = require('./rules-utils.js');

const errMsg = 'Please use a printf-like formatted string that util.format' +
               ' can consume.';

function isArrowFunctionWithTemplateLiteral(node) {
  return node.type === 'ArrowFunctionExpression' &&
         node.body.type === 'TemplateLiteral';
}

module.exports = {
  create: function(context) {
    return {
      ExpressionStatement: function(node) {
        if (!isDefiningError(node) || node.expression.arguments.length < 2)
          return;

        const msg = node.expression.arguments[1];
        if (!isArrowFunctionWithTemplateLiteral(msg))
          return;

        // Checks to see if order of arguments to function is the same as the
        // order of them being concatenated in the template string. The idea is
        // that if both match, then you can use `util.format`-style args.
        // Would pass rule: (a, b) => `${b}${a}`.
        // Would fail rule: (a, b) => `${a}${b}`, and needs to be rewritten.
        const { expressions } = msg.body;
        const hasSequentialParams = msg.params.every((param, index) => {
          const expr = expressions[index];
          return expr && expr.type === 'Identifier' && param.name === expr.name;
        });
        if (hasSequentialParams)
          context.report(msg, errMsg);
      },
    };
  },
};
