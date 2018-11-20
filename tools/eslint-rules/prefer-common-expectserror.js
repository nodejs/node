'use strict';

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

const msg = 'Please use common.expectsError(fn, err) instead of ' +
            'assert.throws(fn, common.expectsError(err)).';

const astSelector =
  'CallExpression[arguments.length=2]' +
                '[callee.object.name="assert"]' +
                '[callee.property.name="throws"]' +
                '[arguments.1.callee.object.name="common"]' +
                '[arguments.1.callee.property.name="expectsError"]';

module.exports = function(context) {
  return {
    [astSelector]: (node) => context.report(node, msg)
  };
};
