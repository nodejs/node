/**
 * @fileoverview A rule to suggest using template literals instead of string concatenation.
 * @author Toru Nagashima
 * @copyright 2015 Toru Nagashima. All rights reserved.
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
 * Gets the top binary expression node for concatenation in parents of a given node.
 * @param {ASTNode} node - A node to get.
 * @returns {ASTNode} the top binary expression node in parents of a given node.
 */
function getTopConcatBinaryExpression(node) {
    while (isConcatenation(node.parent)) {
        node = node.parent;
    }
    return node;
}

/**
 * Checks whether or not a given binary expression has non string literals.
 * @param {ASTNode} node - A node to check.
 * @returns {boolean} `true` if the node has non string literals.
 */
function hasNonStringLiteral(node) {
    if (isConcatenation(node)) {

        // `left` is deeper than `right` normally.
        return hasNonStringLiteral(node.right) || hasNonStringLiteral(node.left);
    }
    return !astUtils.isStringLiteral(node);
}

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = function(context) {
    var done = Object.create(null);

    /**
     * Reports if a given node is string concatenation with non string literals.
     *
     * @param {ASTNode} node - A node to check.
     * @returns {void}
     */
    function checkForStringConcat(node) {
        if (!astUtils.isStringLiteral(node) || !isConcatenation(node.parent)) {
            return;
        }

        var topBinaryExpr = getTopConcatBinaryExpression(node.parent);

        // Checks whether or not this node had been checked already.
        if (done[topBinaryExpr.range[0]]) {
            return;
        }
        done[topBinaryExpr.range[0]] = true;

        if (hasNonStringLiteral(topBinaryExpr)) {
            context.report(
                topBinaryExpr,
                "Unexpected string concatenation.");
        }
    }

    return {
        Program: function() {
            done = Object.create(null);
        },

        Literal: checkForStringConcat,
        TemplateLiteral: checkForStringConcat
    };
};

module.exports.schema = [];
