/**
 * @fileoverview Rule to check empty newline after "var" statement
 * @author Gopal Venkatesan
 * @copyright 2015 Gopal Venkatesan. All rights reserved.
 * @copyright 2015 Casey Visco. All rights reserved.
 * @copyright 2015 Ian VanSchooten. All rights reserved.
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

    // Cache starting and ending line numbers of comments for faster lookup
    var commentEndLine = context.getAllComments().reduce(function(result, token) {
        result[token.loc.start.line] = token.loc.end.line;
        return result;
    }, {});


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
     * Determine if provided nodeType is a function specifier
     * @private
     * @param {string} nodeType - nodeType to test
     * @returns {boolean} True if `nodeType` is a function specifier
     */
    function isFunctionSpecifier(nodeType) {
        return nodeType === "FunctionDeclaration" || nodeType === "FunctionExpression" ||
            nodeType === "ArrowFunctionExpression";
    }

    /**
     * Determine if provided node is the last of his parent
     * @private
     * @param {ASTNode} node - node to test
     * @returns {boolean} True if `node` is last of his parent
     */
    function isLastNode(node) {
        return node.parent.body[node.parent.body.length - 1] === node;
    }

    /**
     * Determine if a token starts more than one line after a comment ends
     * @param  {token}   token            The token being checked
     * @param {integer}  commentStartLine The line number on which the comment starts
     * @returns {boolean}                 True if `token` does not start immediately after a comment
     */
    function hasBlankLineAfterComment(token, commentStartLine) {
        var commentEnd = commentEndLine[commentStartLine];
        // If there's another comment, repeat check for blank line
        if (commentEndLine[commentEnd + 1]) {
            return hasBlankLineAfterComment(token, commentEnd + 1);
        }
        return (token.loc.start.line > commentEndLine[commentStartLine] + 1);
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

        // Ignore if it is last statement in a function
        if (node.parent.parent && isFunctionSpecifier(node.parent.parent.type) && isLastNode(node)) {
            return;
        }

        // Next statement is not a `var`...
        noNextLineToken = nextToken.loc.start.line > nextLineNum;
        hasNextLineComment = (typeof commentEndLine[nextLineNum] !== "undefined");

        if (mode === "never" && noNextLineToken && !hasNextLineComment) {
            context.report(node, NEVER_MESSAGE, { identifier: node.name });
        }

        // Token on the next line, or comment without blank line
        if (
            mode === "always" && (
                !noNextLineToken ||
                hasNextLineComment && !hasBlankLineAfterComment(nextToken, nextLineNum)
            )
        ) {
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
