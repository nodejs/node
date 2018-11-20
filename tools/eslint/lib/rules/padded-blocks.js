/**
 * @fileoverview A rule to ensure blank lines within blocks.
 * @author Mathias Schreck <https://github.com/lo1tuma>
 * @copyright 2014 Mathias Schreck. All rights reserved.
 */

"use strict";

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = function (context) {
    var requirePadding = context.options[0] !== "never";

    var ALWAYS_MESSAGE = "Block must be padded by blank lines.",
        NEVER_MESSAGE = "Block must not be padded by blank lines.";

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
            leadingComments = context.getComments(first).leading;

        if (leadingComments.length > 0) {
            firstLine = leadingComments[0].loc.start.line;
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
            lastLine = context.getLastToken(last).loc.end.line,
            expectedLastLine = blockEnd - 2,
            trailingComments = context.getComments(last).trailing;

        if (trailingComments.length > 0) {
            lastLine = trailingComments[trailingComments.length - 1].loc.end.line;
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
                    context.report(node, node.loc.end, ALWAYS_MESSAGE);
                }
            } else {
                if (blockHasTopPadding) {
                    context.report(node, NEVER_MESSAGE);
                }

                if (blockHasBottomPadding) {
                    context.report(node, node.loc.end, NEVER_MESSAGE);
                }
            }
        }
    }

    return {
        "BlockStatement": checkPadding
    };

};
