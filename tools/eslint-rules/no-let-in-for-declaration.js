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
    const sourceCode = context.getSourceCode();
    const msg = 'Use of `let` as the loop variable in a for-loop is ' +
                'not recommended. Please use `var` instead.';

    /**
     * Report function to test if the for-loop is declared using `let`.
     */
    function testForLoop(node) {
      if (node.init && node.init.kind === 'let') {
        context.report({
          node: node.init,
          message: msg,
          fix: (fixer) =>
            fixer.replaceText(sourceCode.getFirstToken(node.init), 'var')
        });
      }
    }

    /**
     * Report function to test if the for-in or for-of loop
     * is declared using `let`.
     */
    function testForInOfLoop(node) {
      if (node.left && node.left.kind === 'let') {
        context.report({
          node: node.left,
          message: msg,
          fix: (fixer) =>
            fixer.replaceText(sourceCode.getFirstToken(node.left), 'var')
        });
      }
    }

    return {
      'ForStatement': testForLoop,
      'ForInStatement': testForInOfLoop,
      'ForOfStatement': testForInOfLoop
    };
  }
};
