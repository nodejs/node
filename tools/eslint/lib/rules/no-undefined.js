/**
 * @fileoverview Rule to flag references to the undefined variable.
 * @author Michael Ficarra
 */
"use strict";

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = {
    meta: {
        docs: {
            description: "disallow the use of `undefined` as an identifier",
            category: "Variables",
            recommended: false
        },

        schema: []
    },

    create(context) {

        return {

            Identifier(node) {
                if (node.name === "undefined") {
                    const parent = context.getAncestors().pop();

                    if (!parent || parent.type !== "MemberExpression" || node !== parent.property || parent.computed) {
                        context.report(node, "Unexpected use of undefined.");
                    }
                }
            }
        };

    }
};
