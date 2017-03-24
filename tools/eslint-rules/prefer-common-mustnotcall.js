/**
 * @fileoverview Prefer common.mustNotCall(msg) over common.mustCall(fn, 0)
 * @author James M Snell <jasnell@gmail.com>
 */
'use strict';

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

const msg = 'Please use common.mustNotCall(msg) instead of ' +
            'common.mustCall(fn, 0) or common.mustCall(0).';

function isCommonMustCall(node) {
  return node &&
         node.callee &&
         node.callee.object &&
         node.callee.object.name === 'common' &&
         node.callee.property &&
         node.callee.property.name === 'mustCall';
}

function isArgZero(argument) {
  return argument &&
         typeof argument.value === 'number' &&
         argument.value === 0;
}

module.exports = function(context) {
  return {
    CallExpression(node) {
      if (isCommonMustCall(node) &&
          (isArgZero(node.arguments[0]) ||  //  catch common.mustCall(0)
           isArgZero(node.arguments[1]))) { //  catch common.mustCall(fn, 0)
        context.report(node, msg);
      }
    }
  };
};
