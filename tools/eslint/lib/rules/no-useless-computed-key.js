/**
 * @fileoverview Rule to disallow unnecessary computed property keys in object literals
 * @author Burak Yigit Kaya
 */
"use strict";

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

const MESSAGE_UNNECESSARY_COMPUTED = "Unnecessarily computed property [{{property}}] found.";

module.exports = {
    meta: {
        docs: {
            description: "disallow unnecessary computed property keys in object literals",
            category: "ECMAScript 6",
            recommended: false
        },

        schema: []
    },
    create: function(context) {
        const sourceCode = context.getSourceCode();

        return {
            Property: function(node) {
                if (!node.computed) {
                    return;
                }

                const key = node.key,
                    nodeType = typeof key.value;

                if (key.type === "Literal" && (nodeType === "string" || nodeType === "number")) {
                    context.report(node, MESSAGE_UNNECESSARY_COMPUTED, { property: sourceCode.getText(key) });
                }
            }
        };
    }
};
