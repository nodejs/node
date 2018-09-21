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

module.exports = {
  create(context) {

    const msg = 'Use of `let` as the loop variable in a for-loop is ' +
                'not recommended. Please use `var` instead.';

    /**
     * Report function to test if the for-loop is declared using `let`.
     */
    function testForLoop(node) {
      if (node.init && node.init.kind === 'let') {
        context.report(node.init, msg);
      }
    }

    /**
     * Report function to test if the for-in or for-of loop
     * is declared using `let`.
     */
    function testForInOfLoop(node) {
      if (node.left && node.left.kind === 'let') {
        context.report(node.left, msg);
      }
    }

    return {
      'ForStatement': testForLoop,
      'ForInStatement': testForInOfLoop,
      'ForOfStatement': testForInOfLoop
    };
  }
};
