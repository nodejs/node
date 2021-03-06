/**
 * @fileoverview Prohibit use of a single argument only in `assert.fail()`. It
 * is almost always an error.
 * @author Rich Trott
 */
'use strict';

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

const msg = 'assert.fail() message should be third argument';

function isAssert(node) {
  return node.callee.object && node.callee.object.name === 'assert';
}

function isFail(node) {
  return node.callee.property && node.callee.property.name === 'fail';
}

module.exports = function(context) {
  return {
    'CallExpression': function(node) {
      if (isAssert(node) && isFail(node) && node.arguments.length === 1) {
        context.report(node, msg);
      }
    }
  };
};
