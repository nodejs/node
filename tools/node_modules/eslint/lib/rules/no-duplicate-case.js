/**
 * @fileoverview Rule to disallow a duplicate case label.
 * @author Dieter Oberkofler
 * @author Burak Yigit Kaya
 */

"use strict";

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = {
    meta: {
        type: "problem",

        docs: {
            description: "disallow duplicate case labels",
            category: "Possible Errors",
            recommended: true,
            url: "https://eslint.org/docs/rules/no-duplicate-case"
        },

        schema: [],

        messages: {
            unexpected: "Duplicate case label."
        }
    },

    create(context) {
        const sourceCode = context.getSourceCode();

        return {
            SwitchStatement(node) {
                const previousKeys = new Set();

                for (const switchCase of node.cases) {
                    if (switchCase.test) {
                        const key = sourceCode.getText(switchCase.test);

                        if (previousKeys.has(key)) {
                            context.report({ node: switchCase, messageId: "unexpected" });
                        } else {
                            previousKeys.add(key);
                        }
                    }
                }
            }
        };
    }
};
