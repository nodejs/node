/**
 * @fileoverview Rule to disallow use of new operator with the `require` function
 * @author Wil Moore III
 */

"use strict";

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = {
    meta: {
        docs: {
            description: "disallow `new` operators with calls to `require`",
            category: "Node.js and CommonJS",
            recommended: false
        },

        schema: []
    },

    create: function(context) {

        return {

            NewExpression: function(node) {
                if (node.callee.type === "Identifier" && node.callee.name === "require") {
                    context.report(node, "Unexpected use of new with require.");
                }
            }
        };

    }
};
