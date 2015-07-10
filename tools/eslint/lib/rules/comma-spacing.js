/**
 * @fileoverview Comma spacing - validates spacing before and after comma
 * @author Vignesh Anand aka vegetableman.
 * @copyright 2014 Vignesh Anand. All rights reserved.
 */
"use strict";

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = function(context) {

    var options = {
        before: context.options[0] ? !!context.options[0].before : false,
        after: context.options[0] ? !!context.options[0].after : true
    };

    //--------------------------------------------------------------------------
    // Helpers
    //--------------------------------------------------------------------------

    // the index of the last comment that was checked
    var lastCommentIndex = 0;
    var allComments;

    /**
     * Determines the length of comment between 2 tokens
     * @param {Object} left - The left token object.
     * @param {Object} right - The right token object.
     * @returns {number} Length of comment in between tokens
     */
    function getCommentLengthBetweenTokens(left, right) {
        return allComments.reduce(function(val, comment) {
            if (left.range[1] <= comment.range[0] && comment.range[1] <= right.range[0]) {
                val = val + comment.range[1] - comment.range[0];
            }
            return val;
        }, 0);
    }

    /**
     * Determines whether two adjacent tokens have whitespace between them.
     * @param {Object} left - The left token object.
     * @param {Object} right - The right token object.
     * @returns {boolean} Whether or not there is space between the tokens.
     */
    function isSpaced(left, right) {
        var punctuationLength = context.getTokensBetween(left, right).length; // the length of any parenthesis
        var commentLenth = getCommentLengthBetweenTokens(left, right);
        return (left.range[1] + punctuationLength + commentLenth) < right.range[0];
    }

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
     * Reports a spacing error with an appropriate message.
     * @param {ASTNode} node The binary expression node to report.
     * @param {string} dir Is the error "before" or "after" the comma?
     * @returns {void}
     * @private
     */
    function report(node, dir) {
        context.report(node, options[dir] ?
            "A space is required " + dir + " ','." :
            "There should be no space " + dir + " ','.");
    }

    /**
     * Validates the spacing around a comma token.
     * @param {Object} tokens - The tokens to be validated.
     * @param {Token} tokens.comma The token representing the comma.
     * @param {Token} [tokens.left] The last token before the comma.
     * @param {Token} [tokens.right] The first token after the comma.
     * @param {Token|ASTNode} reportItem The item to use when reporting an error.
     * @returns {void}
     * @private
     */
    function validateCommaItemSpacing(tokens, reportItem) {
        if (tokens.left && isSameLine(tokens.left, tokens.comma) &&
                (options.before !== isSpaced(tokens.left, tokens.comma))
        ) {
            report(reportItem, "before");
        }
        if (tokens.right && isSameLine(tokens.comma, tokens.right) &&
                (options.after !== isSpaced(tokens.comma, tokens.right))
        ) {
            report(reportItem, "after");
        }
    }

    /**
     * Determines if a given source index is in a comment or not by checking
     * the index against the comment range. Since the check goes straight
     * through the file, once an index is passed a certain comment, we can
     * go to the next comment to check that.
     * @param {int} index The source index to check.
     * @param {ASTNode[]} comments An array of comment nodes.
     * @returns {boolean} True if the index is within a comment, false if not.
     * @private
     */
    function isIndexInComment(index, comments) {

        var comment;

        while (lastCommentIndex < comments.length) {

            comment = comments[lastCommentIndex];

            if (comment.range[0] <= index && index < comment.range[1]) {
                return true;
            } else if (index > comment.range[1]) {
                lastCommentIndex++;
            } else {
                break;
            }

        }

        return false;
    }

    //--------------------------------------------------------------------------
    // Public
    //--------------------------------------------------------------------------

    return {
        "Program": function() {

            var source = context.getSource(),
                pattern = /,/g,
                commaToken,
                previousToken,
                nextToken;

            allComments = context.getAllComments();
            while (pattern.test(source)) {

                // do not flag anything inside of comments
                if (!isIndexInComment(pattern.lastIndex, allComments)) {
                    commaToken = context.getTokenByRangeStart(pattern.lastIndex - 1);

                    if (commaToken && commaToken.type !== "JSXText") {
                        previousToken = context.getTokenBefore(commaToken);
                        nextToken = context.getTokenAfter(commaToken);
                        validateCommaItemSpacing({
                            comma: commaToken,
                            left: isComma(previousToken) ? null : previousToken,
                            right: isComma(nextToken) ? null : nextToken
                        }, commaToken);
                    }
                }
            }
        }
    };

};

module.exports.schema = [
    {
        "type": "object",
        "properties": {
            "before": {
                "type": "boolean"
            },
            "after": {
                "type": "boolean"
            }
        },
        "additionalProperties": false
    }
];
