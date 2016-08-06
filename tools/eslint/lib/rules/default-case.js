/**
 * @fileoverview require default case in switch statements
 * @author Aliaksei Shytkin
 */
"use strict";

let DEFAULT_COMMENT_PATTERN = /^no default$/;

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = {
    meta: {
        docs: {
            description: "require `default` cases in `switch` statements",
            category: "Best Practices",
            recommended: false
        },

        schema: [{
            type: "object",
            properties: {
                commentPattern: {
                    type: "string"
                }
            },
            additionalProperties: false
        }]
    },

    create: function(context) {
        let options = context.options[0] || {};
        let commentPattern = options.commentPattern ?
            new RegExp(options.commentPattern) :
            DEFAULT_COMMENT_PATTERN;

        let sourceCode = context.getSourceCode();

        //--------------------------------------------------------------------------
        // Helpers
        //--------------------------------------------------------------------------

        /**
         * Shortcut to get last element of array
         * @param  {*[]} collection Array
         * @returns {*} Last element
         */
        function last(collection) {
            return collection[collection.length - 1];
        }

        //--------------------------------------------------------------------------
        // Public
        //--------------------------------------------------------------------------

        return {

            SwitchStatement: function(node) {

                if (!node.cases.length) {

                    /*
                     * skip check of empty switch because there is no easy way
                     * to extract comments inside it now
                     */
                    return;
                }

                let hasDefault = node.cases.some(function(v) {
                    return v.test === null;
                });

                if (!hasDefault) {

                    let comment;
                    let comments;

                    let lastCase = last(node.cases);

                    comments = sourceCode.getComments(lastCase).trailing;

                    if (comments.length) {
                        comment = last(comments);
                    }

                    if (!comment || !commentPattern.test(comment.value.trim())) {
                        context.report(node, "Expected a default case.");
                    }
                }
            }
        };
    }
};
