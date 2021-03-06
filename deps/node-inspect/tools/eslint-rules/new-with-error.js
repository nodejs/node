/**
 * @fileoverview Require `throw new Error()` rather than `throw Error()`
 * @author Rich Trott
 */
'use strict';

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = function(context) {

  var errorList = context.options.length !== 0 ? context.options : ['Error'];

  return {
    'ThrowStatement': function(node) {
      if (node.argument.type === 'CallExpression' &&
          errorList.indexOf(node.argument.callee.name) !== -1) {
        context.report(node, 'Use new keyword when throwing.');
      }
    }
  };
};

module.exports.schema = {
  'type': 'array',
  'additionalItems': {
    'type': 'string'
  },
  'uniqueItems': true
};
