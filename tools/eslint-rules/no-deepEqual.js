/**
 * @fileoverview Prohibit use of assert.deepEqual()
 * @author Rich Trott
 *
 * This rule is imperfect, but will find the most common forms of
 * assert.deepEqual() usage.
 */
'use strict';

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

const msg = 'assert.deepEqual() disallowed. Use assert.deepStrictEqual()';

function isAssert(node) {
  return node.callee.object && node.callee.object.name === 'assert';
}

function isDeepEqual(node) {
  return node.callee.property && node.callee.property.name === 'deepEqual';
}

module.exports = function(context) {
  return {
    'CallExpression': function(node) {
      if (isAssert(node) && isDeepEqual(node)) {
        context.report(node, msg);
      }
    }
  };
};
