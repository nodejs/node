/**
 * @fileoverview Rule to flag creation of function inside a loop
 * @author Ilya Volodin
 * @copyright 2013 Ilya Volodin. All rights reserved.
 */

"use strict";

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = function(context) {
    /**
     * Reports if the given node has an ancestor node which is a loop.
     * @param {ASTNode} node The AST node to check.
     * @returns {boolean} Whether or not the node is within a loop.
     */
    function checkForLoops(node) {
        var ancestors = context.getAncestors();

        /**
         * Checks if the given node is a loop and current context is in the loop.
         * @param {ASTNode} ancestor The AST node to check.
         * @param {number} index The index of ancestor in ancestors.
         * @returns {boolean} Whether or not the node is a loop and current context is in the loop.
         */
        function isInLoop(ancestor, index) {
            switch (ancestor.type) {
                case "ForStatement":
                    return ancestor.init !== ancestors[index + 1];

                case "ForInStatement":
                case "ForOfStatement":
                    return ancestor.right !== ancestors[index + 1];

                case "WhileStatement":
                case "DoWhileStatement":
                    return true;

                default:
                    return false;
            }
        }

        if (ancestors.some(isInLoop)) {
            context.report(node, "Don't make functions within a loop");
        }
    }

    return {
        "ArrowFunctionExpression": checkForLoops,
        "FunctionExpression": checkForLoops,
        "FunctionDeclaration": checkForLoops
    };
};

module.exports.schema = [];
