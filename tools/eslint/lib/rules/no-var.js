/**
 * @fileoverview Rule to check for the usage of var.
 * @author Jamund Ferguson
 */

"use strict";

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = {
    meta: {
        docs: {
            description: "require `let` or `const` instead of `var`",
            category: "ECMAScript 6",
            recommended: false
        },

        schema: []
    },

    create: function(context) {

        return {
            VariableDeclaration: function(node) {
                if (node.kind === "var") {
                    context.report(node, "Unexpected var, use let or const instead.");
                }
            }

        };

    }
};
