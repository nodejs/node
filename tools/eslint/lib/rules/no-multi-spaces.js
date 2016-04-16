/**
 * @fileoverview Disallow use of multiple spaces.
 * @author Nicholas C. Zakas
 * @copyright 2015 Brandon Mills. All rights reserved.
 * @copyright 2015 Nicholas C. Zakas. All rights reserved.
 */

"use strict";

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = function(context) {

    // the index of the last comment that was checked
    var exceptions = { "Property": true },
        hasExceptions = true,
        options = context.options[0],
        lastCommentIndex = 0;

    if (options && options.exceptions) {
        Object.keys(options.exceptions).forEach(function(key) {
            if (options.exceptions[key]) {
                exceptions[key] = true;
            } else {
                delete exceptions[key];
            }
        });
        hasExceptions = Object.keys(exceptions).length > 0;
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
                allComments = context.getAllComments(),
                pattern = /[^\n\r\u2028\u2029\t ].? {2,}/g,  // note: repeating space
                token,
                previousToken,
                parent;


            /**
             * Creates a fix function that removes the multiple spaces between the two tokens
             * @param {RuleFixer} leftToken left token
             * @param {RuleFixer} rightToken right token
             * @returns {function} fix function
             * @private
             */
            function createFix(leftToken, rightToken) {
                return function(fixer) {
                    return fixer.replaceTextRange([leftToken.range[1], rightToken.range[0]], " ");
                };
            }

            while (pattern.test(source)) {

                // do not flag anything inside of comments
                if (!isIndexInComment(pattern.lastIndex, allComments)) {

                    token = context.getTokenByRangeStart(pattern.lastIndex);
                    if (token) {
                        previousToken = context.getTokenBefore(token);

                        if (hasExceptions) {
                            parent = context.getNodeByRangeIndex(pattern.lastIndex - 1);
                        }

                        if (!parent || !exceptions[parent.type]) {
                            context.report({
                                node: token,
                                loc: token.loc.start,
                                message: "Multiple spaces found before '{{value}}'.",
                                data: { value: token.value },
                                fix: createFix(previousToken, token)
                            });
                        }
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
            "exceptions": {
                "type": "object",
                "patternProperties": {
                    "^([A-Z][a-z]*)+$": {
                        "type": "boolean"
                    }
                },
                "additionalProperties": false
            }
        },
        "additionalProperties": false
    }
];
