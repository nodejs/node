/**
 * @fileoverview Operator linebreak - enforces operator linebreak style of two types: after and before
 * @author Benoît Zugmeyer
 */

"use strict";

var lodash = require("lodash"),
    astUtils = require("../ast-utils");

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = {
    meta: {
        docs: {
            description: "enforce consistent linebreak style for operators",
            category: "Stylistic Issues",
            recommended: false
        },

        schema: [
            {
                enum: ["after", "before", "none", null]
            },
            {
                type: "object",
                properties: {
                    overrides: {
                        type: "object",
                        properties: {
                            anyOf: {
                                type: "string",
                                enum: ["after", "before", "none", "ignore"]
                            }
                        }
                    }
                },
                additionalProperties: false
            }
        ]
    },

    create: function(context) {

        var usedDefaultGlobal = !context.options[0];
        var globalStyle = context.options[0] || "after";
        var options = context.options[1] || {};
        var styleOverrides = options.overrides ? lodash.assign({}, options.overrides) : {};

        if (usedDefaultGlobal && !styleOverrides["?"]) {
            styleOverrides["?"] = "before";
        }

        if (usedDefaultGlobal && !styleOverrides[":"]) {
            styleOverrides[":"] = "before";
        }

        //--------------------------------------------------------------------------
        // Helpers
        //--------------------------------------------------------------------------

        /**
         * Checks the operator placement
         * @param {ASTNode} node The node to check
         * @param {ASTNode} leftSide The node that comes before the operator in `node`
         * @private
         * @returns {void}
         */
        function validateNode(node, leftSide) {
            var leftToken = context.getLastToken(leftSide);
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
            var operatorStyleOverride = styleOverrides[operator];
            var style = operatorStyleOverride || globalStyle;

            // if single line
            if (astUtils.isTokenOnSameLine(leftToken, operatorToken) &&
                    astUtils.isTokenOnSameLine(operatorToken, rightToken)) {

                return;

            } else if (operatorStyleOverride !== "ignore" && !astUtils.isTokenOnSameLine(leftToken, operatorToken) &&
                    !astUtils.isTokenOnSameLine(operatorToken, rightToken)) {

                // lone operator
                context.report(node, {
                    line: operatorToken.loc.end.line,
                    column: operatorToken.loc.end.column
                }, "Bad line breaking before and after '" + operator + "'.");

            } else if (style === "before" && astUtils.isTokenOnSameLine(leftToken, operatorToken)) {

                context.report(node, {
                    line: operatorToken.loc.end.line,
                    column: operatorToken.loc.end.column
                }, "'" + operator + "' should be placed at the beginning of the line.");

            } else if (style === "after" && astUtils.isTokenOnSameLine(operatorToken, rightToken)) {

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

        /**
         * Validates a binary expression using `validateNode`
         * @param {BinaryExpression|LogicalExpression|AssignmentExpression} node node to be validated
         * @returns {void}
         */
        function validateBinaryExpression(node) {
            validateNode(node, node.left);
        }

        //--------------------------------------------------------------------------
        // Public
        //--------------------------------------------------------------------------

        return {
            BinaryExpression: validateBinaryExpression,
            LogicalExpression: validateBinaryExpression,
            AssignmentExpression: validateBinaryExpression,
            VariableDeclarator: function(node) {
                if (node.init) {
                    validateNode(node, node.id);
                }
            },
            ConditionalExpression: function(node) {
                validateNode(node, node.test);
                validateNode(node, node.consequent);
            }
        };
    }
};
