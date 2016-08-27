/**
 * @fileoverview Rule to flag when using constructor without parentheses
 * @author Ilya Volodin
 */

"use strict";

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = {
    meta: {
        docs: {
            description: "require parentheses when invoking a constructor with no arguments",
            category: "Stylistic Issues",
            recommended: false
        },

        schema: []
    },

    create(context) {
        const sourceCode = context.getSourceCode();

        return {

            NewExpression(node) {
                const tokens = sourceCode.getTokens(node);
                const prenticesTokens = tokens.filter(function(token) {
                    return token.value === "(" || token.value === ")";
                });

                if (prenticesTokens.length < 2) {
                    context.report(node, "Missing '()' invoking a constructor.");
                }
            }
        };

    }
};
