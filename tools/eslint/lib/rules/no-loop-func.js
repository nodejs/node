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
    var loopNodeTypes = [
        "ForStatement",
        "WhileStatement",
        "ForInStatement",
        "ForOfStatement",
        "DoWhileStatement"
    ];

    /**
     * Checks if the given node is a loop.
     * @param {ASTNode} node The AST node to check.
     * @returns {boolean} Whether or not the node is a loop.
     */
    function isLoop(node) {
        return loopNodeTypes.indexOf(node.type) > -1;
    }

    /**
     * Reports if the given node has an ancestor node which is a loop.
     * @param {ASTNode} node The AST node to check.
     * @returns {boolean} Whether or not the node is within a loop.
     */
    function checkForLoops(node) {
        var ancestors = context.getAncestors();

        if (ancestors.some(isLoop)) {
            context.report(node, "Don't make functions within a loop");
        }
    }

    return {
        "ArrowFunctionExpression": checkForLoops,
        "FunctionExpression": checkForLoops,
        "FunctionDeclaration": checkForLoops
    };
};
