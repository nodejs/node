/**
 * @fileoverview Rule to disallow whitespace before the semicolon
 * @author Jonathan Kingston
 * @copyright 2015 Mathias Schreck
 * @copyright 2014 Jonathan Kingston
 */

"use strict";

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = function(context) {

    /**
     * Determines whether two adjacent tokens are have whitespace between them.
     * @param {Object} left - The left token object.
     * @param {Object} right - The right token object.
     * @returns {boolean} Whether or not there is space between the tokens.
     */
    function isSpaced(left, right) {
        return left.range[1] < right.range[0];
    }

    /**
     * Checks whether two tokens are on the same line.
     * @param {Object} left The leftmost token.
     * @param {Object} right The rightmost token.
     * @returns {boolean} True if the tokens are on the same line, false if not.
     * @private
     */
    function isSameLine(left, right) {
        return left.loc.end.line === right.loc.start.line;
    }

    /**
     * Checks if a given token has leading whitespace.
     * @param {Object} token The token to check.
     * @returns {boolean} True if the given token has leading space, false if not.
     */
    function hasLeadingSpace(token) {
        var tokenBefore = context.getTokenBefore(token);
        return isSameLine(tokenBefore, token) && isSpaced(tokenBefore, token);
    }

    /**
     * Checks if the given token is a semicolon.
     * @param {Token} token The token to check.
     * @returns {boolean} Whether or not the given token is a semicolon.
     */
    function isSemicolon(token) {
        return token.type === "Punctuator" && token.value === ";";
    }

    /**
     * Reports if the given token has leading space.
     * @param {Token} token The semicolon token to check.
     * @param {ASTNode} node The corresponding node of the token.
     * @returns {void}
     */
    function checkSemiTokenForLeadingSpace(token, node) {
        if (isSemicolon(token) && hasLeadingSpace(token)) {
            context.report(node, token.loc.start, "Unexpected whitespace before semicolon.");
        }
    }

    /**
     * Checks leading space before the semicolon with the assumption that the last token is the semicolon.
     * @param {ASTNode} node The node to check.
     * @returns {void}
     */
    function checkNode(node) {
        var token = context.getLastToken(node);
        checkSemiTokenForLeadingSpace(token, node);
    }

    return {
        "VariableDeclaration": checkNode,
        "ExpressionStatement": checkNode,
        "BreakStatement": checkNode,
        "ContinueStatement": checkNode,
        "DebuggerStatement": checkNode,
        "ReturnStatement": checkNode,
        "ThrowStatement": checkNode,
        "ForStatement": function (node) {
            if (node.init) {
                checkSemiTokenForLeadingSpace(context.getTokenAfter(node.init), node);
            }

            if (node.test) {
                checkSemiTokenForLeadingSpace(context.getTokenAfter(node.test), node);
            }
        }
    };
};

module.exports.schema = [];
