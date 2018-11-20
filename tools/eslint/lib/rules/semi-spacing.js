/**
 * @fileoverview Validates spacing before and after semicolon
 * @author Mathias Schreck
 * @copyright 2015 Mathias Schreck
 */

"use strict";

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = function (context) {

    var config = context.options[0],
        requireSpaceBefore = false,
        requireSpaceAfter = true;

    if (typeof config === "object") {
        if (config.hasOwnProperty("before")) {
            requireSpaceBefore = config.before;
        }
        if (config.hasOwnProperty("after")) {
            requireSpaceAfter = config.after;
        }
    }

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
        return tokenBefore && isSameLine(tokenBefore, token) && isSpaced(tokenBefore, token);
    }

    /**
     * Checks if a given token has trailing whitespace.
     * @param {Object} token The token to check.
     * @returns {boolean} True if the given token has trailing space, false if not.
     */
    function hasTrailingSpace(token) {
        var tokenAfter = context.getTokenAfter(token);
        return tokenAfter && isSameLine(token, tokenAfter) && isSpaced(token, tokenAfter);
    }

    /**
     * Checks if the given token is the last token in its line.
     * @param {Token} token The token to check.
     * @returns {boolean} Whether or not the token is the last in its line.
     */
    function isLastTokenInCurrentLine(token) {
        var tokenAfter = context.getTokenAfter(token);
        return !(tokenAfter && isSameLine(token, tokenAfter));
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
     * Reports if the given token has invalid spacing.
     * @param {Token} token The semicolon token to check.
     * @param {ASTNode} node The corresponding node of the token.
     * @returns {void}
     */
    function checkSemicolonSpacing(token, node) {
        var location;

        if (isSemicolon(token)) {
            location = token.loc.start;

            if (hasLeadingSpace(token)) {
                if (!requireSpaceBefore) {
                    context.report(node, location, "Unexpected whitespace before semicolon.");
                }
            } else {
                if (requireSpaceBefore) {
                    context.report(node, location, "Missing whitespace before semicolon.");
                }
            }

            if (!isLastTokenInCurrentLine(token)) {
                if (hasTrailingSpace(token)) {
                    if (!requireSpaceAfter) {
                        context.report(node, location, "Unexpected whitespace after semicolon.");
                    }
                } else {
                    if (requireSpaceAfter) {
                        context.report(node, location, "Missing whitespace after semicolon.");
                    }
                }
            }
        }
    }

    /**
     * Checks the spacing of the semicolon with the assumption that the last token is the semicolon.
     * @param {ASTNode} node The node to check.
     * @returns {void}
     */
    function checkNode(node) {
        var token = context.getLastToken(node);
        checkSemicolonSpacing(token, node);
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
                checkSemicolonSpacing(context.getTokenAfter(node.init), node);
            }

            if (node.test) {
                checkSemicolonSpacing(context.getTokenAfter(node.test), node);
            }
        }
    };
};
