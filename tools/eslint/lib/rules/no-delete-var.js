/**
 * @fileoverview Rule to flag when deleting variables
 * @author Ilya Volodin
 */

"use strict";

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = {
    meta: {
        docs: {
            description: "disallow deleting variables",
            category: "Variables",
            recommended: true
        },

        schema: []
    },

    create: function(context) {

        return {

            UnaryExpression: function(node) {
                if (node.operator === "delete" && node.argument.type === "Identifier") {
                    context.report(node, "Variables should not be deleted.");
                }
            }
        };

    }
};
