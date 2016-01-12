/**
 * @fileoverview Rule to flag missing semicolons.
 * @author Nicholas C. Zakas
 * @copyright 2013 Nicholas C. Zakas. All rights reserved.
 * See LICENSE file in root directory for full license.
 */
"use strict";

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = function(context) {

    var OPT_OUT_PATTERN = /[\[\(\/\+\-]/; // One of [(/+-

    var always = context.options[0] !== "never",
        sourceCode = context.getSourceCode();

    //--------------------------------------------------------------------------
    // Helpers
    //--------------------------------------------------------------------------

    /**
     * Reports a semicolon error with appropriate location and message.
     * @param {ASTNode} node The node with an extra or missing semicolon.
     * @returns {void}
     */
    function report(node) {
        var message,
            fix,
            lastToken = sourceCode.getLastToken(node),
            loc = lastToken.loc;

        if (always) {
            message = "Missing semicolon.";
            loc = loc.end;
            fix = function(fixer) {
                return fixer.insertTextAfter(lastToken, ";");
            };
        } else {
            message = "Extra semicolon.";
            loc = loc.start;
            fix = function(fixer) {
                return fixer.remove(lastToken);
            };
        }

        context.report({
            node: node,
            loc: loc,
            message: message,
            fix: fix
        });

    }

    /**
     * Checks whether a token is a semicolon punctuator.
     * @param {Token} token The token.
     * @returns {boolean} True if token is a semicolon punctuator.
     */
    function isSemicolon(token) {
        return (token.type === "Punctuator" && token.value === ";");
    }

    /**
     * Check if a semicolon is unnecessary, only true if:
     *   - next token is on a new line and is not one of the opt-out tokens
     *   - next token is a valid statement divider
     * @param {Token} lastToken last token of current node.
     * @returns {boolean} whether the semicolon is unnecessary.
     */
    function isUnnecessarySemicolon(lastToken) {
        var isDivider, isOptOutToken, lastTokenLine, nextToken, nextTokenLine;

        if (!isSemicolon(lastToken)) {
            return false;
        }

        nextToken = context.getTokenAfter(lastToken);

        if (!nextToken) {
            return true;
        }

        lastTokenLine = lastToken.loc.end.line;
        nextTokenLine = nextToken.loc.start.line;
        isOptOutToken = OPT_OUT_PATTERN.test(nextToken.value);
        isDivider = (nextToken.value === "}" || nextToken.value === ";");

        return (lastTokenLine !== nextTokenLine && !isOptOutToken) || isDivider;
    }

    /**
     * Checks a node to see if it's followed by a semicolon.
     * @param {ASTNode} node The node to check.
     * @returns {void}
     */
    function checkForSemicolon(node) {
        var lastToken = context.getLastToken(node);

        if (always) {
            if (!isSemicolon(lastToken)) {
                report(node);
            }
        } else {
            if (isUnnecessarySemicolon(lastToken)) {
                report(node);
            }
        }
    }

    /**
     * Checks to see if there's a semicolon after a variable declaration.
     * @param {ASTNode} node The node to check.
     * @returns {void}
     */
    function checkForSemicolonForVariableDeclaration(node) {
        var ancestors = context.getAncestors(),
            parentIndex = ancestors.length - 1,
            parent = ancestors[parentIndex];

        if ((parent.type !== "ForStatement" || parent.init !== node) &&
            (!/^For(?:In|Of)Statement/.test(parent.type) || parent.left !== node)
        ) {
            checkForSemicolon(node);
        }
    }

    //--------------------------------------------------------------------------
    // Public API
    //--------------------------------------------------------------------------

    return {

        "VariableDeclaration": checkForSemicolonForVariableDeclaration,
        "ExpressionStatement": checkForSemicolon,
        "ReturnStatement": checkForSemicolon,
        "ThrowStatement": checkForSemicolon,
        "DoWhileStatement": checkForSemicolon,
        "DebuggerStatement": checkForSemicolon,
        "BreakStatement": checkForSemicolon,
        "ContinueStatement": checkForSemicolon,
        "ImportDeclaration": checkForSemicolon,
        "ExportAllDeclaration": checkForSemicolon,
        "ExportNamedDeclaration": function(node) {
            if (!node.declaration) {
                checkForSemicolon(node);
            }
        },
        "ExportDefaultDeclaration": function(node) {
            if (!/(?:Class|Function)Declaration/.test(node.declaration.type)) {
                checkForSemicolon(node);
            }
        }
    };

};

module.exports.schema = [
    {
        "enum": ["always", "never"]
    }
];
