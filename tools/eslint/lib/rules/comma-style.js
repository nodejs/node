/**
 * @fileoverview Comma style - enforces comma styles of two types: last and first
 * @author Vignesh Anand aka vegetableman
 * @copyright 2014 Vignesh Anand. All rights reserved.
 * @copyright 2015 Evan Simmons. All rights reserved.
 */

"use strict";

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = function(context) {

    var style = context.options[0] || "last",
        exceptions = {};

    if (context.options.length === 2 && context.options[1].hasOwnProperty("exceptions")) {
        exceptions = context.options[1].exceptions;
    }

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
        if (isSameLine(commaToken, currentItemToken) &&
                isSameLine(previousItemToken, commaToken)) {

            return;

        } else if (!isSameLine(commaToken, currentItemToken) &&
                !isSameLine(previousItemToken, commaToken)) {

            // lone comma
            context.report(reportItem, {
                line: commaToken.loc.end.line,
                column: commaToken.loc.start.column
            }, "Bad line breaking before and after ','.");

        } else if (style === "first" && !isSameLine(commaToken, currentItemToken)) {

            context.report(reportItem, "',' should be placed first.");

        } else if (style === "last" && isSameLine(commaToken, currentItemToken)) {

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
        var items = node[property],
            arrayLiteral = (node.type === "ArrayExpression"),
            previousItemToken;

        if (items.length > 1 || arrayLiteral) {

            // seed as opening [
            previousItemToken = context.getFirstToken(node);

            items.forEach(function(item) {
                var commaToken = item ? context.getTokenBefore(item) : previousItemToken,
                    currentItemToken = item ? context.getFirstToken(item) : context.getTokenAfter(commaToken),
                    reportItem = item || currentItemToken;

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

                previousItemToken = item ? context.getLastToken(item) : previousItemToken;
            });

            /*
             * Special case for array literals that have empty last items, such
             * as [ 1, 2, ]. These arrays only have two items show up in the
             * AST, so we need to look at the token to verify that there's no
             * dangling comma.
             */
            if (arrayLiteral) {

                var lastToken = context.getLastToken(node),
                    nextToLastToken = context.getTokenBefore(lastToken);

                if (isComma(nextToLastToken)) {
                    validateCommaItemSpacing(
                        context.getTokenBefore(nextToLastToken),
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

    var nodes = {};

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
};
