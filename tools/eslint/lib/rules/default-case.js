/**
 * @fileoverview require default case in switch statements
 * @author Aliaksei Shytkin
 */
"use strict";

var COMMENT_VALUE = "no default";

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = function(context) {

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

        "SwitchStatement": function(node) {

            if (!node.cases.length) {
                // skip check of empty switch because there is no easy way
                // to extract comments inside it now
                return;
            }

            var hasDefault = node.cases.some(function(v) {
                return v.test === null;
            });

            if (!hasDefault) {

                var comment;
                var comments;

                var lastCase = last(node.cases);
                comments = context.getComments(lastCase).trailing;

                if (comments.length) {
                    comment = last(comments);
                }

                if (!comment || comment.value.trim() !== COMMENT_VALUE) {
                    context.report(node, "Expected a default case.");
                }
            }
        }
    };
};

module.exports.schema = [];
