/**
 * @fileoverview Rule to flag when initializing octal literal
 * @author Ilya Volodin
 */

"use strict";

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = {
    meta: {
        docs: {
            description: "disallow octal literals",
            category: "Best Practices",
            recommended: true
        },

        schema: []
    },

    create: function(context) {

        return {

            Literal: function(node) {
                if (typeof node.value === "number" && /^0[0-7]/.test(node.raw)) {
                    context.report(node, "Octal literals should not be used.");
                }
            }
        };

    }
};
