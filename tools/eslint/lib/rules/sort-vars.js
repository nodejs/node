/**
 * @fileoverview Rule to require sorting of variables within a single Variable Declaration block
 * @author Ilya Volodin
 */

"use strict";

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = {
    meta: {
        docs: {
            description: "require variables within the same declaration block to be sorted",
            category: "Stylistic Issues",
            recommended: false
        },

        schema: [
            {
                type: "object",
                properties: {
                    ignoreCase: {
                        type: "boolean"
                    }
                },
                additionalProperties: false
            }
        ]
    },

    create: function(context) {

        const configuration = context.options[0] || {},
            ignoreCase = configuration.ignoreCase || false;

        return {
            VariableDeclaration: function(node) {
                node.declarations.reduce(function(memo, decl) {
                    if (decl.id.type === "ObjectPattern" || decl.id.type === "ArrayPattern") {
                        return memo;
                    }

                    let lastVariableName = memo.id.name,
                        currenVariableName = decl.id.name;

                    if (ignoreCase) {
                        lastVariableName = lastVariableName.toLowerCase();
                        currenVariableName = currenVariableName.toLowerCase();
                    }

                    if (currenVariableName < lastVariableName) {
                        context.report(decl, "Variables within the same declaration block should be sorted alphabetically.");
                        return memo;
                    } else {
                        return decl;
                    }
                }, node.declarations[0]);
            }
        };
    }
};
