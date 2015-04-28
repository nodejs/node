/**
 * @fileoverview Rule to flag when using constructor without parentheses
 * @author Ilya Volodin
 */

"use strict";

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = function(context) {

    return {

        "NewExpression": function(node) {
            var tokens = context.getTokens(node);
            var prenticesTokens = tokens.filter(function(token) {
                return token.value === "(" || token.value === ")";
            });
            if (prenticesTokens.length < 2) {
                context.report(node, "Missing '()' invoking a constructor");
            }
        }
    };

};
