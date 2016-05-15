/**
 * @fileoverview Rule to flag unsafe statements in finally block
 * @author Onur Temizkan
 */

"use strict";

//------------------------------------------------------------------------------
// Helpers
//------------------------------------------------------------------------------

var SENTINEL_NODE_TYPE_RETURN_THROW = /^(?:Program|(?:Function|Class)(?:Declaration|Expression)|ArrowFunctionExpression)$/;
var SENTINEL_NODE_TYPE_BREAK = /^(?:Program|(?:Function|Class)(?:Declaration|Expression)|ArrowFunctionExpression|DoWhileStatement|WhileStatement|ForOfStatement|ForInStatement|ForStatement|SwitchStatement)$/;
var SENTINEL_NODE_TYPE_CONTINUE = /^(?:Program|(?:Function|Class)(?:Declaration|Expression)|ArrowFunctionExpression|DoWhileStatement|WhileStatement|ForOfStatement|ForInStatement|ForStatement)$/;


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
         * @param {String} label - label of the break or continue statement
         * @returns {Boolean} - return whether the node is a finally block or a sentinel node
         */
        function isInFinallyBlock(node, label) {
            var labelInside = false;
            var sentinelNodeType;

            if (node.type === "BreakStatement" && !node.label) {
                sentinelNodeType = SENTINEL_NODE_TYPE_BREAK;
            } else if (node.type === "ContinueStatement") {
                sentinelNodeType = SENTINEL_NODE_TYPE_CONTINUE;
            } else {
                sentinelNodeType = SENTINEL_NODE_TYPE_RETURN_THROW;
            }

            while (node && !sentinelNodeType.test(node.type)) {
                if (node.parent.label && label && (node.parent.label.name === label.name)) {
                    labelInside = true;
                }
                if (isFinallyBlock(node)) {
                    if (label && labelInside) {
                        return false;
                    }
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
            if (isInFinallyBlock(node, node.label)) {
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
