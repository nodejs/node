/**
 * @file Prefer common.mustNotCall(msg) over common.mustCall(fn, 0)
 * @author James M Snell <jasnell@gmail.com>
 */
'use strict';

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

const msg = 'Please use common.mustNotCall(msg) instead of ' +
            'common.mustCall(fn, 0) or common.mustCall(0).';
const mustCallSelector = 'CallExpression[callee.object.name="common"]' +
                         '[callee.property.name="mustCall"]';
const arg0Selector = `${mustCallSelector}[arguments.0.value=0]`;
const arg1Selector = `${mustCallSelector}[arguments.1.value=0]`;

module.exports = {
  create(context) {
    function report(node) {
      context.report(node, msg);
    }

    return {
    // Catch common.mustCall(0)
      [arg0Selector]: report,

      // Catch common.mustCall(fn, 0)
      [arg1Selector]: report,
    };
  },
};
