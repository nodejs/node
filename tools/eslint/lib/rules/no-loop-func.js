/**
 * @fileoverview Rule to flag creation of function inside a loop
 * @author Ilya Volodin
 * @copyright 2013 Ilya Volodin. All rights reserved.
 */

"use strict";

//------------------------------------------------------------------------------
// Helpers
//------------------------------------------------------------------------------

/**
 * Gets the containing loop node of a specified node.
 *
 * We don't need to check nested functions, so this ignores those.
 * `Scope.through` contains references of nested functions.
 *
 * @param {ASTNode} node - An AST node to get.
 * @returns {ASTNode|null} The containing loop node of the specified node, or `null`.
 */
function getContainingLoopNode(node) {
    var parent = node.parent;
    while (parent) {
        switch (parent.type) {
            case "WhileStatement":
            case "DoWhileStatement":
                return parent;

            case "ForStatement":
                // `init` is outside of the loop.
                if (parent.init !== node) {
                    return parent;
                }
                break;

            case "ForInStatement":
            case "ForOfStatement":
                // `right` is outside of the loop.
                if (parent.right !== node) {
                    return parent;
                }
                break;

            case "ArrowFunctionExpression":
            case "FunctionExpression":
            case "FunctionDeclaration":
                // We don't need to check nested functions.
                return null;

            default:
                break;
        }

        node = parent;
        parent = node.parent;
    }

    return null;
}

/**
 * Checks whether or not a reference refers to a variable that is block-binding in the loop.
 * @param {ASTNode} loopNode - A containing loop node.
 * @param {escope.Reference} reference - A reference to check.
 * @returns {boolean} Whether or not a reference refers to a variable that is block-binding in the loop.
 */
function isBlockBindingsInLoop(loopNode, reference) {
    // A reference to a `let`/`const` variable always has a resolved variable.
    var variable = reference.resolved;
    var definition = variable && variable.defs[0];
    var declaration = definition && definition.parent;

    return (
        // Checks whether this is `let`/`const`.
        declaration &&
        declaration.type === "VariableDeclaration" &&
        (declaration.kind === "let" || declaration.kind === "const") &&
        // Checks whether this is in the loop.
        declaration.range[0] > loopNode.range[0] &&
        declaration.range[1] < loopNode.range[1]
    );
}

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = function(context) {
    /**
     * Reports such functions:
     *
     * - has an ancestor node which is a loop.
     * - has a reference that refers to a variable that is block-binding in the loop.
     *
     * @param {ASTNode} node The AST node to check.
     * @returns {boolean} Whether or not the node is within a loop.
     */
    function checkForLoops(node) {
        var loopNode = getContainingLoopNode(node);
        if (!loopNode) {
            return;
        }

        var references = context.getScope().through;
        if (references.length > 0 && !references.every(isBlockBindingsInLoop.bind(null, loopNode))) {
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
