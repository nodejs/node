/**
 * @fileoverview Disallow shadowing of NaN, undefined, and Infinity (ES5 section 15.1.1)
 * @author Michael Ficarra
 */
"use strict";

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = {
    meta: {
        type: "suggestion",

        docs: {
            description: "disallow identifiers from shadowing restricted names",
            category: "Variables",
            recommended: false,
            url: "https://eslint.org/docs/rules/no-shadow-restricted-names"
        },

        schema: []
    },

    create(context) {

        const RESTRICTED = ["undefined", "NaN", "Infinity", "arguments", "eval"];

        return {
            "VariableDeclaration, :function, CatchClause"(node) {
                for (const variable of context.getDeclaredVariables(node)) {
                    if (variable.defs.length > 0 && RESTRICTED.includes(variable.name)) {
                        context.report({
                            node: variable.defs[0].name,
                            message: "Shadowing of global property '{{idName}}'.",
                            data: {
                                idName: variable.name
                            }
                        });
                    }
                }
            }
        };

    }
};
