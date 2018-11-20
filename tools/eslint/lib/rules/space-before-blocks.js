/**
 * @fileoverview A rule to ensure whitespace before blocks.
 * @author Mathias Schreck <https://github.com/lo1tuma>
 * @copyright 2014 Mathias Schreck. All rights reserved.
 */

"use strict";

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = function (context) {
    var requireSpace = context.options[0] !== "never";

    /**
     * Determines whether two adjacent tokens are have whitespace between them.
     * @param {Object} left - The left token object.
     * @param {Object} right - The right token object.
     * @returns {boolean} Whether or not there is space between the tokens.
     */
    function isSpaced(left, right) {
        return left.range[1] < right.range[0];
    }

    /**
     * Determines whether two adjacent tokens are on the same line.
     * @param {Object} left - The left token object.
     * @param {Object} right - The right token object.
     * @returns {boolean} Whether or not the tokens are on the same line.
     */
    function isSameLine(left, right) {
        return left.loc.start.line === right.loc.start.line;
    }

    /**
     * Checks the given BlockStatement node has a preceding space if it doesn’t start on a new line.
     * @param {ASTNode|Token} node The AST node of a BlockStatement.
     * @returns {void} undefined.
     */
    function checkPrecedingSpace(node) {
        var precedingToken = context.getTokenBefore(node),
            hasSpace;

        if (precedingToken && isSameLine(precedingToken, node)) {
            hasSpace = isSpaced(precedingToken, node);

            if (requireSpace) {
                if (!hasSpace) {
                    context.report(node, "Missing space before opening brace.");
                }
            } else {
                if (hasSpace) {
                    context.report(node, "Unexpected space before opening brace.");
                }
            }
        }
    }

    /**
     * Checks if the CaseBlock of an given SwitchStatement node has a preceding space.
     * @param {ASTNode} node The node of a SwitchStatement.
     * @returns {void} undefined.
     */
    function checkSpaceBeforeCaseBlock(node) {
        var cases = node.cases,
            firstCase,
            openingBrace;

        if (cases.length > 0) {
            firstCase = cases[0];
            openingBrace = context.getTokenBefore(firstCase);
        } else {
            openingBrace = context.getLastToken(node, 1);
        }

        checkPrecedingSpace(openingBrace);
    }

    return {
        "BlockStatement": checkPrecedingSpace,
        "SwitchStatement": checkSpaceBeforeCaseBlock
    };

};
