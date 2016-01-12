/**
 * @fileoverview Rule to forbid or enforce dangling commas.
 * @author Ian Christian Myers
 * @copyright 2015 Toru Nagashima
 * @copyright 2015 Mathias Schreck
 * @copyright 2013 Ian Christian Myers
 * See LICENSE file in root directory for full license.
 */

"use strict";

//------------------------------------------------------------------------------
// Helpers
//------------------------------------------------------------------------------

/**
 * Gets the last element of a given array.
 *
 * @param {*[]} xs - An array to get.
 * @returns {*} The last element, or undefined.
 */
function getLast(xs) {
    if (xs.length === 0) {
        return null;
    }
    return xs[xs.length - 1];
}

/**
 * Checks whether or not a trailing comma is allowed in a given node.
 * `ArrayPattern` which has `RestElement` disallows it.
 *
 * @param {ASTNode} node - A node to check.
 * @param {ASTNode} lastItem - The node of the last element in the given node.
 * @returns {boolean} `true` if a trailing comma is allowed.
 */
function isTrailingCommaAllowed(node, lastItem) {
    switch (node.type) {
        case "ArrayPattern":
            // TODO(t-nagashima): Remove SpreadElement after https://github.com/eslint/espree/issues/194 was fixed.
            return (
                lastItem.type !== "RestElement" &&
                lastItem.type !== "SpreadElement"
            );

        // TODO(t-nagashima): Remove this case after https://github.com/eslint/espree/issues/195 was fixed.
        case "ArrayExpression":
            return (
                node.parent.type !== "ForOfStatement" ||
                node.parent.left !== node ||
                lastItem.type !== "SpreadElement"
            );

        default:
            return true;
    }
}

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = function(context) {
    var mode = context.options[0];
    var UNEXPECTED_MESSAGE = "Unexpected trailing comma.";
    var MISSING_MESSAGE = "Missing trailing comma.";

    /**
     * Checks whether or not a given node is multiline.
     * This rule handles a given node as multiline when the closing parenthesis
     * and the last element are not on the same line.
     *
     * @param {ASTNode} node - A ndoe to check.
     * @returns {boolean} `true` if the node is multiline.
     */
    function isMultiline(node) {
        var lastItem = getLast(node.properties || node.elements || node.specifiers);
        if (!lastItem) {
            return false;
        }

        var sourceCode = context.getSourceCode(),
            penultimateToken = sourceCode.getLastToken(lastItem),
            lastToken = sourceCode.getLastToken(node);

        if (lastToken.value === ",") {
            penultimateToken = lastToken;
            lastToken = sourceCode.getTokenAfter(lastToken);
        }

        return lastToken.loc.end.line !== penultimateToken.loc.end.line;
    }

    /**
     * Reports a trailing comma if it exists.
     *
     * @param {ASTNode} node - A node to check. Its type is one of
     *   ObjectExpression, ObjectPattern, ArrayExpression, ArrayPattern,
     *   ImportDeclaration, and ExportNamedDeclaration.
     * @returns {void}
     */
    function forbidTrailingComma(node) {
        var lastItem = getLast(node.properties || node.elements || node.specifiers);
        if (!lastItem || (node.type === "ImportDeclaration" && lastItem.type !== "ImportSpecifier")) {
            return;
        }

        var sourceCode = context.getSourceCode(),
            trailingToken;

        // last item can be surrounded by parentheses for object and array literals
        if (node.type === "ObjectExpression" || node.type === "ArrayExpression") {
            trailingToken = sourceCode.getTokenBefore(sourceCode.getLastToken(node));
        } else {
            trailingToken = sourceCode.getTokenAfter(lastItem);
        }

        if (trailingToken.value === ",") {
            context.report(
                lastItem,
                trailingToken.loc.start,
                UNEXPECTED_MESSAGE);
        }
    }

    /**
     * Reports the last element of a given node if it does not have a trailing
     * comma.
     *
     * If a given node is `ArrayPattern` which has `RestElement`, the trailing
     * comma is disallowed, so report if it exists.
     *
     * @param {ASTNode} node - A node to check. Its type is one of
     *   ObjectExpression, ObjectPattern, ArrayExpression, ArrayPattern,
     *   ImportDeclaration, and ExportNamedDeclaration.
     * @returns {void}
     */
    function forceTrailingComma(node) {
        var lastItem = getLast(node.properties || node.elements || node.specifiers);
        if (!lastItem || (node.type === "ImportDeclaration" && lastItem.type !== "ImportSpecifier")) {
            return;
        }
        if (!isTrailingCommaAllowed(node, lastItem)) {
            forbidTrailingComma(node);
            return;
        }

        var sourceCode = context.getSourceCode(),
            trailingToken;

        // last item can be surrounded by parentheses for object and array literals
        if (node.type === "ObjectExpression" || node.type === "ArrayExpression") {
            trailingToken = sourceCode.getTokenBefore(sourceCode.getLastToken(node));
        } else {
            trailingToken = sourceCode.getTokenAfter(lastItem);
        }

        if (trailingToken.value !== ",") {
            context.report(
                lastItem,
                lastItem.loc.end,
                MISSING_MESSAGE);
        }
    }

    /**
     * If a given node is multiline, reports the last element of a given node
     * when it does not have a trailing comma.
     * Otherwise, reports a trailing comma if it exists.
     *
     * @param {ASTNode} node - A node to check. Its type is one of
     *   ObjectExpression, ObjectPattern, ArrayExpression, ArrayPattern,
     *   ImportDeclaration, and ExportNamedDeclaration.
     * @returns {void}
     */
    function forceTrailingCommaIfMultiline(node) {
        if (isMultiline(node)) {
            forceTrailingComma(node);
        } else {
            forbidTrailingComma(node);
        }
    }

    // Chooses a checking function.
    var checkForTrailingComma;
    if (mode === "always") {
        checkForTrailingComma = forceTrailingComma;
    } else if (mode === "always-multiline") {
        checkForTrailingComma = forceTrailingCommaIfMultiline;
    } else {
        checkForTrailingComma = forbidTrailingComma;
    }

    return {
        "ObjectExpression": checkForTrailingComma,
        "ObjectPattern": checkForTrailingComma,
        "ArrayExpression": checkForTrailingComma,
        "ArrayPattern": checkForTrailingComma,
        "ImportDeclaration": checkForTrailingComma,
        "ExportNamedDeclaration": checkForTrailingComma
    };
};

module.exports.schema = [
    {
        "enum": ["always", "always-multiline", "never"]
    }
];
