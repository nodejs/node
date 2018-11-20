'use strict';

const astSelector = "CallExpression[callee.name='isNaN']";
const msg = 'Please use Number.isNaN instead of the global isNaN function';

module.exports = function(context) {
  function report(node) {
    context.report(node, msg);
  }

  return {
    [astSelector]: report
  };
};
