/**
 * @fileoverview Validates spacing before and after semicolon
 * @author Mathias Schreck
 */

"use strict";

var astUtils = require("../ast-utils");

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = {
    meta: {
        docs: {
            description: "enforce consistent spacing before and after semicolons",
            category: "Stylistic Issues",
            recommended: false
        },

        fixable: "whitespace",

        schema: [
            {
                type: "object",
                properties: {
                    before: {
                        type: "boolean"
                    },
                    after: {
                        type: "boolean"
                    }
                },
                additionalProperties: false
            }
        ]
    },

    create: function(context) {

        var config = context.options[0],
            requireSpaceBefore = false,
            requireSpaceAfter = true,
            sourceCode = context.getSourceCode();

        if (typeof config === "object") {
            if (config.hasOwnProperty("before")) {
                requireSpaceBefore = config.before;
            }
            if (config.hasOwnProperty("after")) {
                requireSpaceAfter = config.after;
            }
        }

        /**
         * Checks if a given token has leading whitespace.
         * @param {Object} token The token to check.
         * @returns {boolean} True if the given token has leading space, false if not.
         */
        function hasLeadingSpace(token) {
            var tokenBefore = context.getTokenBefore(token);

            return tokenBefore && astUtils.isTokenOnSameLine(tokenBefore, token) && sourceCode.isSpaceBetweenTokens(tokenBefore, token);
        }

        /**
         * Checks if a given token has trailing whitespace.
         * @param {Object} token The token to check.
         * @returns {boolean} True if the given token has trailing space, false if not.
         */
        function hasTrailingSpace(token) {
            var tokenAfter = context.getTokenAfter(token);

            return tokenAfter && astUtils.isTokenOnSameLine(token, tokenAfter) && sourceCode.isSpaceBetweenTokens(token, tokenAfter);
        }

        /**
         * Checks if the given token is the last token in its line.
         * @param {Token} token The token to check.
         * @returns {boolean} Whether or not the token is the last in its line.
         */
        function isLastTokenInCurrentLine(token) {
            var tokenAfter = context.getTokenAfter(token);

            return !(tokenAfter && astUtils.isTokenOnSameLine(token, tokenAfter));
        }

        /**
         * Checks if the given token is the first token in its line
         * @param {Token} token The token to check.
         * @returns {boolean} Whether or not the token is the first in its line.
         */
        function isFirstTokenInCurrentLine(token) {
            var tokenBefore = context.getTokenBefore(token);

            return !(tokenBefore && astUtils.isTokenOnSameLine(token, tokenBefore));
        }

        /**
         * Checks if the next token of a given token is a closing parenthesis.
         * @param {Token} token The token to check.
         * @returns {boolean} Whether or not the next token of a given token is a closing parenthesis.
         */
        function isBeforeClosingParen(token) {
            var nextToken = context.getTokenAfter(token);

            return (
                nextToken &&
                nextToken.type === "Punctuator" &&
                (nextToken.value === "}" || nextToken.value === ")")
            );
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
                        context.report({
                            node: node,
                            loc: location,
                            message: "Unexpected whitespace before semicolon.",
                            fix: function(fixer) {
                                var tokenBefore = context.getTokenBefore(token);

                                return fixer.removeRange([tokenBefore.range[1], token.range[0]]);
                            }
                        });
                    }
                } else {
                    if (requireSpaceBefore) {
                        context.report({
                            node: node,
                            loc: location,
                            message: "Missing whitespace before semicolon.",
                            fix: function(fixer) {
                                return fixer.insertTextBefore(token, " ");
                            }
                        });
                    }
                }

                if (!isFirstTokenInCurrentLine(token) && !isLastTokenInCurrentLine(token) && !isBeforeClosingParen(token)) {
                    if (hasTrailingSpace(token)) {
                        if (!requireSpaceAfter) {
                            context.report({
                                node: node,
                                loc: location,
                                message: "Unexpected whitespace after semicolon.",
                                fix: function(fixer) {
                                    var tokenAfter = context.getTokenAfter(token);

                                    return fixer.removeRange([token.range[1], tokenAfter.range[0]]);
                                }
                            });
                        }
                    } else {
                        if (requireSpaceAfter) {
                            context.report({
                                node: node,
                                loc: location,
                                message: "Missing whitespace after semicolon.",
                                fix: function(fixer) {
                                    return fixer.insertTextAfter(token, " ");
                                }
                            });
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
            VariableDeclaration: checkNode,
            ExpressionStatement: checkNode,
            BreakStatement: checkNode,
            ContinueStatement: checkNode,
            DebuggerStatement: checkNode,
            ReturnStatement: checkNode,
            ThrowStatement: checkNode,
            ForStatement: function(node) {
                if (node.init) {
                    checkSemicolonSpacing(context.getTokenAfter(node.init), node);
                }

                if (node.test) {
                    checkSemicolonSpacing(context.getTokenAfter(node.test), node);
                }
            }
        };
    }
};
