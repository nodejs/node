/**
 * @fileoverview Prohibit the use of `let` as the loop variable
 *               in the initialization of for, and the left-hand
 *               iterator in forIn and forOf loops.
 *
 * @author Jessica Quynh Tran
 */

'use strict';

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------
const message = 'Use of `let` as the loop variable in a for-loop is ' +
                'not recommended. Please use `var` instead.';
const forSelector = 'ForStatement[init.kind="let"]';
const forInOfSelector = 'ForOfStatement[left.kind="let"],' +
                        'ForInStatement[left.kind="let"]';

module.exports = {
  create(context) {
    const sourceCode = context.getSourceCode();

    function report(node) {
      context.report({
        node,
        message,
        fix: (fixer) =>
          fixer.replaceText(sourceCode.getFirstToken(node), 'var')
      });
    }

    return {
      [forSelector]: (node) => report(node.init),
      [forInOfSelector]: (node) => report(node.left),
    };
  }
};
