/**
 * @fileoverview A rule to ensure blank lines within blocks.
 * @author Mathias Schreck <https://github.com/lo1tuma>
 * @copyright 2014 Mathias Schreck. All rights reserved.
 */

"use strict";

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = function(context) {
    var requirePadding = context.options[0] !== "never";

    var ALWAYS_MESSAGE = "Block must be padded by blank lines.",
        NEVER_MESSAGE = "Block must not be padded by blank lines.";

    /**
     * Checks if the location of a node or token is before the location of another node or token
     * @param {ASTNode|Token} a The node or token to check if its location is before b.
     * @param {ASTNode|Token} b The node or token which will be compared with a.
     * @returns {boolean} True if a is located before b.
     */
    function isLocatedBefore(a, b) {
        return a.range[1] < b.range[0];
    }

    /**
     * Checks if the given non empty block node has a blank line before its first child node.
     * @param {ASTNode} node The AST node of a BlockStatement.
     * @returns {boolean} Whether or not the block starts with a blank line.
     */
    function isBlockTopPadded(node) {
        var blockStart = node.loc.start.line,
            first = node.body[0],
            firstLine = first.loc.start.line,
            expectedFirstLine = blockStart + 2,
            leadingComments = (node.body[0].leadingComments || []).slice(),
            firstComment;

        while (leadingComments.length > 0 &&
                leadingComments[0].loc.start.line <= node.loc.start.line) {
            leadingComments.shift();
        }

        firstComment = leadingComments[0];

        if (firstComment && isLocatedBefore(firstComment, first)) {
            firstLine = firstComment.loc.start.line;
        }

        return expectedFirstLine <= firstLine;
    }

    /**
     * Checks if the given non empty block node has a blank line after its last child node.
     * @param {ASTNode} node The AST node of a BlockStatement.
     * @returns {boolean} Whether or not the block ends with a blank line.
     */
    function isBlockBottomPadded(node) {
        var blockEnd = node.loc.end.line,
            last = node.body[node.body.length - 1],
            lastToken = context.getLastToken(last),
            lastLine = lastToken.loc.end.line,
            expectedLastLine = blockEnd - 2,
            trailingComments = (node.body[node.body.length - 1].trailingComments || []).slice(),
            lastComment;

        while (trailingComments.length > 0 &&
                trailingComments[trailingComments.length - 1].loc.end.line >= node.loc.end.line) {
            trailingComments.pop();
        }

        lastComment = trailingComments[trailingComments.length - 1];

        if (lastComment && isLocatedBefore(lastToken, lastComment)) {
            lastLine = lastComment.loc.end.line;
        }

        return lastLine <= expectedLastLine;
    }

    /**
     * Checks the given BlockStatement node to be padded if the block is not empty.
     * @param {ASTNode} node The AST node of a BlockStatement.
     * @returns {void} undefined.
     */
    function checkPadding(node) {
        if (node.body.length > 0) {

            var blockHasTopPadding = isBlockTopPadded(node),
                blockHasBottomPadding = isBlockBottomPadded(node);

            if (requirePadding) {
                if (!blockHasTopPadding) {
                    context.report(node, ALWAYS_MESSAGE);
                }
                if (!blockHasBottomPadding) {
                    context.report({
                        node: node,
                        loc: {line: node.loc.end.line, column: node.loc.end.column - 1 },
                        message: ALWAYS_MESSAGE
                    });
                }
            } else {
                if (blockHasTopPadding) {
                    context.report(node, NEVER_MESSAGE);
                }

                if (blockHasBottomPadding) {
                    context.report({
                        node: node,
                        loc: {line: node.loc.end.line, column: node.loc.end.column - 1 },
                        message: NEVER_MESSAGE
                    });
                }
            }
        }
    }

    return {
        "BlockStatement": checkPadding
    };

};

module.exports.schema = [
    {
        "enum": ["always", "never"]
    }
];
