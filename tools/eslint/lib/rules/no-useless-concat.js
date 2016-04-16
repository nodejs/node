/**
 * @fileoverview disallow unncessary concatenation of template strings
 * @author Henry Zhu
 * @copyright 2015 Henry Zhu. All rights reserved.
 * See LICENSE file in root directory for full license.
 */
"use strict";

//------------------------------------------------------------------------------
// Requirements
//------------------------------------------------------------------------------

var astUtils = require("../ast-utils");

//------------------------------------------------------------------------------
// Helpers
//------------------------------------------------------------------------------

/**
 * Checks whether or not a given node is a concatenation.
 * @param {ASTNode} node - A node to check.
 * @returns {boolean} `true` if the node is a concatenation.
 */
function isConcatenation(node) {
    return node.type === "BinaryExpression" && node.operator === "+";
}

/**
 * Get's the right most node on the left side of a BinaryExpression with + operator.
 * @param {ASTNode} node - A BinaryExpression node to check.
 * @returns {ASTNode} node
 */
function getLeft(node) {
    var left = node.left;

    while (isConcatenation(left)) {
        left = left.right;
    }
    return left;
}

/**
 * Get's the left most node on the right side of a BinaryExpression with + operator.
 * @param {ASTNode} node - A BinaryExpression node to check.
 * @returns {ASTNode} node
 */
function getRight(node) {
    var right = node.right;

    while (isConcatenation(right)) {
        right = right.left;
    }
    return right;
}

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = function(context) {
    return {
        BinaryExpression: function(node) {

            // check if not concatenation
            if (node.operator !== "+") {
                return;
            }

            // account for the `foo + "a" + "b"` case
            var left = getLeft(node);
            var right = getRight(node);

            if (astUtils.isStringLiteral(left) &&
                astUtils.isStringLiteral(right) &&
                astUtils.isTokenOnSameLine(left, right)
            ) {

                // move warning location to operator
                var operatorToken = context.getTokenAfter(left);

                while (operatorToken.value !== "+") {
                    operatorToken = context.getTokenAfter(operatorToken);
                }

                context.report(
                    node,
                    operatorToken.loc.start,
                    "Unexpected string concatenation of literals.");
            }
        }
    };
};

module.exports.schema = [];
