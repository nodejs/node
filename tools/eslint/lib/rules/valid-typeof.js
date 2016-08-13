/**
 * @fileoverview Ensures that the results of typeof are compared against a valid string
 * @author Ian Christian Myers
 */
"use strict";

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = {
    meta: {
        docs: {
            description: "enforce comparing `typeof` expressions against valid strings",
            category: "Possible Errors",
            recommended: true
        },

        schema: []
    },

    create: function(context) {

        const VALID_TYPES = ["symbol", "undefined", "object", "boolean", "number", "string", "function"],
            OPERATORS = ["==", "===", "!=", "!=="];

        //--------------------------------------------------------------------------
        // Public
        //--------------------------------------------------------------------------

        return {

            UnaryExpression: function(node) {
                if (node.operator === "typeof") {
                    const parent = context.getAncestors().pop();

                    if (parent.type === "BinaryExpression" && OPERATORS.indexOf(parent.operator) !== -1) {
                        const sibling = parent.left === node ? parent.right : parent.left;

                        if (sibling.type === "Literal" && VALID_TYPES.indexOf(sibling.value) === -1) {
                            context.report(sibling, "Invalid typeof comparison value.");
                        }
                    }
                }
            }

        };

    }
};
