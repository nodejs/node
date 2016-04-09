
/**
 * @fileoverview Rule to require newlines before `return` statement
 * @author Kai Cataldo
 * @copyright 2016 Kai Cataldo. All rights reserved.
 * See LICENSE file in root directory for full license.
 */
"use strict";

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = function(context) {
    var sourceCode = context.getSourceCode();

    //--------------------------------------------------------------------------
    // Helpers
    //--------------------------------------------------------------------------

    /**
     * Tests whether node is preceded by supplied tokens
     * @param {ASTNode} node - node to check
     * @param {array} testTokens - array of tokens to test against
     * @returns {boolean} Whether or not the node is preceded by one of the supplied tokens
     * @private
     */
    function isPrecededByTokens(node, testTokens) {
        var tokenBefore = sourceCode.getTokenBefore(node);

        return testTokens.some(function(token) {
            return tokenBefore.value === token;
        });
    }

    /**
     * Checks whether node is the first node after statement or in block
     * @param {ASTNode} node - node to check
     * @returns {boolean} Whether or not the node is the first node after statement or in block
     * @private
     */
    function isFirstNode(node) {
        var parentType = node.parent.type;

        if (node.parent.body) {
            return Array.isArray(node.parent.body)
              ? node.parent.body[0] === node
              : node.parent.body === node;
        }

        if (parentType === "IfStatement") {
            return isPrecededByTokens(node, ["else", ")"]);
        } else if (parentType === "DoWhileStatement") {
            return isPrecededByTokens(node, ["do"]);
        } else if (parentType === "SwitchCase") {
            return isPrecededByTokens(node, [":"]);
        } else {
            return isPrecededByTokens(node, [")"]);
        }
    }

    /**
     * Returns the number of lines of comments that precede the node
     * @param {ASTNode} node - node to check for overlapping comments
     * @param {token} tokenBefore - previous token to check for overlapping comments
     * @returns {number} Number of lines of comments that precede the node
     * @private
     */
    function calcCommentLines(node, tokenBefore) {
        var comments = sourceCode.getComments(node).leading;
        var numLinesComments = 0;

        if (!comments.length) {
            return numLinesComments;
        }

        comments.forEach(function(comment) {
            numLinesComments++;

            if (comment.type === "Block") {
                numLinesComments += comment.loc.end.line - comment.loc.start.line;
            }

            // avoid counting lines with inline comments twice
            if (comment.loc.start.line === tokenBefore.loc.end.line) {
                numLinesComments--;
            }

            if (comment.loc.end.line === node.loc.start.line) {
                numLinesComments--;
            }
        });

        return numLinesComments;
    }

    /**
     * Checks whether node is preceded by a newline
     * @param {ASTNode} node - node to check
     * @returns {boolean} Whether or not the node is preceded by a newline
     * @private
     */
    function hasNewlineBefore(node) {
        var tokenBefore = sourceCode.getTokenBefore(node);
        var lineNumTokenBefore = tokenBefore.loc.end.line;
        var lineNumNode = node.loc.start.line;
        var commentLines = calcCommentLines(node, tokenBefore);

        return (lineNumNode - lineNumTokenBefore - commentLines) > 1;
    }

    /**
     * Reports expected/unexpected newline before return statement
     * @param {ASTNode} node - the node to report in the event of an error
     * @param {boolean} isExpected - whether the newline is expected or not
     * @returns {void}
     * @private
     */
    function reportError(node, isExpected) {
        var expected = isExpected ? "Expected" : "Unexpected";

        context.report({
            node: node,
            message: expected + " newline before return statement."
        });
    }

    //--------------------------------------------------------------------------
    // Public
    //--------------------------------------------------------------------------

    return {
        ReturnStatement: function(node) {
            if (isFirstNode(node) && hasNewlineBefore(node)) {
                reportError(node, false);
            } else if (!isFirstNode(node) && !hasNewlineBefore(node)) {
                reportError(node, true);
            }
        }
    };
};

module.exports.schema = [];
