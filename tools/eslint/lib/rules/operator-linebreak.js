/**
 * @fileoverview Operator linebreak - enforces operator linebreak style of two types: after and before
 * @author Benoît Zugmeyer
 * @copyright 2015 Benoît Zugmeyer. All rights reserved.
 */

"use strict";

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = function(context) {

    var style = context.options[0] || "after";

    //--------------------------------------------------------------------------
    // Helpers
    //--------------------------------------------------------------------------

    /**
     * Checks whether two tokens are on the same line.
     * @param {ASTNode} left The leftmost token.
     * @param {ASTNode} right The rightmost token.
     * @returns {boolean} True if the tokens are on the same line, false if not.
     * @private
     */
    function isSameLine(left, right) {
        return left.loc.end.line === right.loc.start.line;
    }

    /**
     * Checks the operator placement
     * @param {ASTNode} node The binary operator node to check
     * @private
     * @returns {void}
     */
    function validateBinaryExpression(node) {
        var leftToken = context.getLastToken(node.left || node.id);
        var operatorToken = context.getTokenAfter(leftToken);

        // When the left part of a binary expression is a single expression wrapped in
        // parentheses (ex: `(a) + b`), leftToken will be the last token of the expression
        // and operatorToken will be the closing parenthesis.
        // The leftToken should be the last closing parenthesis, and the operatorToken
        // should be the token right after that.
        while (operatorToken.value === ")") {
            leftToken = operatorToken;
            operatorToken = context.getTokenAfter(operatorToken);
        }

        var rightToken = context.getTokenAfter(operatorToken);
        var operator = operatorToken.value;

        // if single line
        if (isSameLine(leftToken, operatorToken) &&
                isSameLine(operatorToken, rightToken)) {

            return;

        } else if (!isSameLine(leftToken, operatorToken) &&
                !isSameLine(operatorToken, rightToken)) {

            // lone operator
            context.report(node, {
                line: operatorToken.loc.end.line,
                column: operatorToken.loc.end.column
            }, "Bad line breaking before and after '" + operator + "'.");

        } else if (style === "before" && isSameLine(leftToken, operatorToken)) {

            context.report(node, {
                line: operatorToken.loc.end.line,
                column: operatorToken.loc.end.column
            }, "'" + operator + "' should be placed at the beginning of the line.");

        } else if (style === "after" && isSameLine(operatorToken, rightToken)) {

            context.report(node, {
                line: operatorToken.loc.end.line,
                column: operatorToken.loc.end.column
            }, "'" + operator + "' should be placed at the end of the line.");

        } else if (style === "none") {

            context.report(node, {
                line: operatorToken.loc.end.line,
                column: operatorToken.loc.end.column
            }, "There should be no line break before or after '" + operator + "'");

        }
    }

    //--------------------------------------------------------------------------
    // Public
    //--------------------------------------------------------------------------

    return {
        "BinaryExpression": validateBinaryExpression,
        "LogicalExpression": validateBinaryExpression,
        "AssignmentExpression": validateBinaryExpression,
        "VariableDeclarator": function (node) {
            if (node.init) {
                validateBinaryExpression(node);
            }
        }
    };
};

module.exports.schema = [
    {
        "enum": ["after", "before", "none"]
    }
];
