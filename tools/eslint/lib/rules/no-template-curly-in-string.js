/**
 * @fileoverview Warn when using template string syntax in regular strings
 * @author Jeroen Engels
 */
"use strict";

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = {
    meta: {
        docs: {
            description: "Disallow template literal placeholder syntax in regular strings",
            category: "Possible Errors",
            recommended: false
        },

        schema: []
    },

    create: function(context) {
        const regex = /\$\{[^}]+\}/;

        return {
            Literal: function(node) {
                if (typeof node.value === "string" && regex.test(node.value)) {
                    context.report({
                        node: node,
                        message: "Unexpected template string expression."
                    });
                }
            }
        };

    }
};
