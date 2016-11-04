/**
 * @fileoverview Require at least two arguments when calling setTimeout() or
 * setInterval().
 * @author Rich Trott
 */
'use strict';

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

function isTimer(name) {
  return ['setTimeout', 'setInterval'].includes(name);
}

module.exports = function(context) {
  return {
    'CallExpression': function(node) {
      const name = node.callee.name;
      if (isTimer(name) && node.arguments.length < 2) {
        context.report(node, `${name} must have at least 2 arguments`);
      }
    }
  };
};
