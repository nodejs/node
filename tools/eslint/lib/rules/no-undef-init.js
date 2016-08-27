/**
 * @fileoverview Rule to flag when initializing to undefined
 * @author Ilya Volodin
 */

"use strict";

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = {
    meta: {
        docs: {
            description: "disallow initializing variables to `undefined`",
            category: "Variables",
            recommended: false
        },

        schema: []
    },

    create(context) {

        return {

            VariableDeclarator(node) {
                const name = node.id.name,
                    init = node.init && node.init.name;

                if (init === "undefined" && node.parent.kind !== "const") {
                    context.report(node, "It's not necessary to initialize '{{name}}' to undefined.", { name });
                }
            }
        };

    }
};
