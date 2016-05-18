/**
 * @fileoverview Restrict usage of specified node imports.
 * @author Guy Ellis
 */
"use strict";

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = {
    meta: {
        docs: {
            description: "disallow specified modules when loaded by `import`",
            category: "ECMAScript 6",
            recommended: false
        },

        schema: {
            type: "array",
            items: {
                type: "string"
            },
            uniqueItems: true
        }
    },

    create: function(context) {
        var restrictedImports = context.options;

        // if no imports are restricted we don"t need to check
        if (restrictedImports.length === 0) {
            return {};
        }

        return {
            ImportDeclaration: function(node) {
                if (node && node.source && node.source.value) {

                    var value = node.source.value.trim();

                    if (restrictedImports.indexOf(value) !== -1) {
                        context.report(node, "'{{importName}}' import is restricted from being used.", {
                            importName: value
                        });
                    }
                }
            }
        };
    }
};
