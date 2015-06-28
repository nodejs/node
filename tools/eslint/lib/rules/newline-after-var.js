/**
 * @fileoverview Rule to check empty newline after "var" statement
 * @author Gopal Venkatesan
 * @copyright 2015 Gopal Venkatesan. All rights reserved.
 * @copyright 2015 Casey Visco. All rights reserved.
 */

"use strict";

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = function(context) {

    var ALWAYS_MESSAGE = "Expected blank line after variable declarations.",
        NEVER_MESSAGE = "Unexpected blank line after variable declarations.";

    // Default `mode` to "always". This means that invalid options will also
    // be treated as "always" and the only special case is "never"
    var mode = context.options[0] === "never" ? "never" : "always";

    // Cache line numbers of comments for faster lookup
    var comments = context.getAllComments().map(function (token) {
        return token.loc.start.line;
    });


    //--------------------------------------------------------------------------
    // Helpers
    //--------------------------------------------------------------------------

    /**
     * Determine if provided keyword is a variable declaration
     * @private
     * @param {string} keyword - keyword to test
     * @returns {boolean} True if `keyword` is a type of var
     */
    function isVar(keyword) {
        return keyword === "var" || keyword === "let" || keyword === "const";
    }

    /**
     * Determine if provided keyword is a variant of for specifiers
     * @private
     * @param {string} keyword - keyword to test
     * @returns {boolean} True if `keyword` is a variant of for specifier
     */
    function isForTypeSpecifier(keyword) {
        return keyword === "ForStatement" || keyword === "ForInStatement" || keyword === "ForOfStatement";
    }

    /**
     * Determine if provided keyword is an export specifiers
     * @private
     * @param {string} nodeType - nodeType to test
     * @returns {boolean} True if `nodeType` is an export specifier
     */
    function isExportSpecifier(nodeType) {
        return nodeType === "ExportNamedDeclaration" || nodeType === "ExportSpecifier" ||
            nodeType === "ExportDefaultDeclaration" || nodeType === "ExportAllDeclaration";
    }

    /**
     * Checks that a blank line exists after a variable declaration when mode is
     * set to "always", or checks that there is no blank line when mode is set
     * to "never"
     * @private
     * @param {ASTNode} node - `VariableDeclaration` node to test
     * @returns {void}
     */
    function checkForBlankLine(node) {
        var lastToken = context.getLastToken(node),
            nextToken = context.getTokenAfter(node),
            nextLineNum = lastToken.loc.end.line + 1,
            noNextLineToken,
            hasNextLineComment;

        // Ignore if there is no following statement
        if (!nextToken) {
            return;
        }

        // Ignore if parent of node is a for variant
        if (isForTypeSpecifier(node.parent.type)) {
            return;
        }

        // Ignore if parent of node is an export specifier
        if (isExportSpecifier(node.parent.type)) {
            return;
        }

        // Some coding styles use multiple `var` statements, so do nothing if
        // the next token is a `var` statement.
        if (nextToken.type === "Keyword" && isVar(nextToken.value)) {
            return;
        }

        // Next statement is not a `var`...
        noNextLineToken = nextToken.loc.start.line > nextLineNum;
        hasNextLineComment = comments.indexOf(nextLineNum) >= 0;

        if (mode === "never" && noNextLineToken && !hasNextLineComment) {
            context.report(node, NEVER_MESSAGE, { identifier: node.name });
        }

        if (mode === "always" && (!noNextLineToken || hasNextLineComment)) {
            context.report(node, ALWAYS_MESSAGE, { identifier: node.name });
        }
    }

    //--------------------------------------------------------------------------
    // Public
    //--------------------------------------------------------------------------

    return {
        "VariableDeclaration": checkForBlankLine
    };

};

module.exports.schema = [
    {
        "enum": ["never", "always"]
    }
];
