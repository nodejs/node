/**
 * @fileoverview Require use of new Buffer constructor methods in lib
 * @author James M Snell
 */
'use strict';

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------
const msg = 'Use of the Buffer() constructor has been deprecated. ' +
            'Please use either Buffer.alloc(), Buffer.allocUnsafe(), ' +
            'or Buffer.from()';
const astSelector = 'NewExpression[callee.name="Buffer"],' +
                    'CallExpression[callee.name="Buffer"]';

module.exports = function(context) {
  return {
    [astSelector]: (node) => context.report(node, msg)
  };
};
