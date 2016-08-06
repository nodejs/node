/**
 * @fileoverview Comma style - enforces comma styles of two types: last and first
 * @author Vignesh Anand aka vegetableman
 */

"use strict";

let astUtils = require("../ast-utils");

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = {
    meta: {
        docs: {
            description: "enforce consistent comma style",
            category: "Stylistic Issues",
            recommended: false
        },

        schema: [
            {
                enum: ["first", "last"]
            },
            {
                type: "object",
                properties: {
                    exceptions: {
                        type: "object",
                        additionalProperties: {
                            type: "boolean"
                        }
                    }
                },
                additionalProperties: false
            }
        ]
    },

    create: function(context) {
        let style = context.options[0] || "last",
            exceptions = {},
            sourceCode = context.getSourceCode();

        if (context.options.length === 2 && context.options[1].hasOwnProperty("exceptions")) {
            exceptions = context.options[1].exceptions;
        }

        //--------------------------------------------------------------------------
        // Helpers
        //--------------------------------------------------------------------------

        /**
         * Determines if a given token is a comma operator.
         * @param {ASTNode} token The token to check.
         * @returns {boolean} True if the token is a comma, false if not.
         * @private
         */
        function isComma(token) {
            return !!token && (token.type === "Punctuator") && (token.value === ",");
        }

        /**
         * Validates the spacing around single items in lists.
         * @param {Token} previousItemToken The last token from the previous item.
         * @param {Token} commaToken The token representing the comma.
         * @param {Token} currentItemToken The first token of the current item.
         * @param {Token} reportItem The item to use when reporting an error.
         * @returns {void}
         * @private
         */
        function validateCommaItemSpacing(previousItemToken, commaToken, currentItemToken, reportItem) {

            // if single line
            if (astUtils.isTokenOnSameLine(commaToken, currentItemToken) &&
                    astUtils.isTokenOnSameLine(previousItemToken, commaToken)) {

                return;

            } else if (!astUtils.isTokenOnSameLine(commaToken, currentItemToken) &&
                    !astUtils.isTokenOnSameLine(previousItemToken, commaToken)) {

                // lone comma
                context.report(reportItem, {
                    line: commaToken.loc.end.line,
                    column: commaToken.loc.start.column
                }, "Bad line breaking before and after ','.");

            } else if (style === "first" && !astUtils.isTokenOnSameLine(commaToken, currentItemToken)) {

                context.report(reportItem, "',' should be placed first.");

            } else if (style === "last" && astUtils.isTokenOnSameLine(commaToken, currentItemToken)) {

                context.report(reportItem, {
                    line: commaToken.loc.end.line,
                    column: commaToken.loc.end.column
                }, "',' should be placed last.");
            }
        }

        /**
         * Checks the comma placement with regards to a declaration/property/element
         * @param {ASTNode} node The binary expression node to check
         * @param {string} property The property of the node containing child nodes.
         * @private
         * @returns {void}
         */
        function validateComma(node, property) {
            let items = node[property],
                arrayLiteral = (node.type === "ArrayExpression"),
                previousItemToken;

            if (items.length > 1 || arrayLiteral) {

                // seed as opening [
                previousItemToken = sourceCode.getFirstToken(node);

                items.forEach(function(item) {
                    let commaToken = item ? sourceCode.getTokenBefore(item) : previousItemToken,
                        currentItemToken = item ? sourceCode.getFirstToken(item) : sourceCode.getTokenAfter(commaToken),
                        reportItem = item || currentItemToken,
                        tokenBeforeComma = sourceCode.getTokenBefore(commaToken);

                    // Check if previous token is wrapped in parentheses
                    if (tokenBeforeComma && tokenBeforeComma.value === ")") {
                        previousItemToken = tokenBeforeComma;
                    }

                    /*
                     * This works by comparing three token locations:
                     * - previousItemToken is the last token of the previous item
                     * - commaToken is the location of the comma before the current item
                     * - currentItemToken is the first token of the current item
                     *
                     * These values get switched around if item is undefined.
                     * previousItemToken will refer to the last token not belonging
                     * to the current item, which could be a comma or an opening
                     * square bracket. currentItemToken could be a comma.
                     *
                     * All comparisons are done based on these tokens directly, so
                     * they are always valid regardless of an undefined item.
                     */
                    if (isComma(commaToken)) {
                        validateCommaItemSpacing(previousItemToken, commaToken,
                                currentItemToken, reportItem);
                    }

                    previousItemToken = item ? sourceCode.getLastToken(item) : previousItemToken;
                });

                /*
                 * Special case for array literals that have empty last items, such
                 * as [ 1, 2, ]. These arrays only have two items show up in the
                 * AST, so we need to look at the token to verify that there's no
                 * dangling comma.
                 */
                if (arrayLiteral) {

                    let lastToken = sourceCode.getLastToken(node),
                        nextToLastToken = sourceCode.getTokenBefore(lastToken);

                    if (isComma(nextToLastToken)) {
                        validateCommaItemSpacing(
                            sourceCode.getTokenBefore(nextToLastToken),
                            nextToLastToken,
                            lastToken,
                            lastToken
                        );
                    }
                }
            }
        }

        //--------------------------------------------------------------------------
        // Public
        //--------------------------------------------------------------------------

        let nodes = {};

        if (!exceptions.VariableDeclaration) {
            nodes.VariableDeclaration = function(node) {
                validateComma(node, "declarations");
            };
        }
        if (!exceptions.ObjectExpression) {
            nodes.ObjectExpression = function(node) {
                validateComma(node, "properties");
            };
        }
        if (!exceptions.ArrayExpression) {
            nodes.ArrayExpression = function(node) {
                validateComma(node, "elements");
            };
        }

        return nodes;
    }
};
