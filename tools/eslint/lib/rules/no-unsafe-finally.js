/**
 * @fileoverview Rule to flag unsafe statements in finally block
 * @author Onur Temizkan
 */

"use strict";

//------------------------------------------------------------------------------
// Helpers
//------------------------------------------------------------------------------

var SENTINEL_NODE_TYPE = /^(?:Program|(?:Function|Class)(?:Declaration|Expression)|ArrowFunctionExpression)$/;

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = {
    meta: {
        docs: {
            description: "disallow control flow statements in finally blocks",
            category: "Possible Errors",
            recommended: false
        }
    },
    create: function(context) {

        /**
         * Checks if the node is the finalizer of a TryStatement
         *
         * @param {ASTNode} node - node to check.
         * @returns {Boolean} - true if the node is the finalizer of a TryStatement
         */
        function isFinallyBlock(node) {
            return node.parent.type === "TryStatement" && node.parent.finalizer === node;
        }

        /**
         * Climbs up the tree if the node is not a sentinel node
         *
         * @param {ASTNode} node - node to check.
         * @returns {Boolean} - return whether the node is a finally block or a sentinel node
         */
        function isInFinallyBlock(node) {
            while (node && !SENTINEL_NODE_TYPE.test(node.type)) {
                if (isFinallyBlock(node)) {
                    return true;
                }
                node = node.parent;
            }
            return false;
        }

        /**
         * Checks whether the possibly-unsafe statement is inside a finally block.
         *
         * @param {ASTNode} node - node to check.
         * @returns {void}
         */
        function check(node) {
            if (isInFinallyBlock(node)) {
                context.report({
                    message: "Unsafe usage of " + node.type,
                    node: node,
                    line: node.loc.line,
                    column: node.loc.column
                });
            }
        }

        return {
            ReturnStatement: check,
            ThrowStatement: check,
            BreakStatement: check,
            ContinueStatement: check
        };
    }
};
