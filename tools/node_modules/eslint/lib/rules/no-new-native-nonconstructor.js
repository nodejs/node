/**
 * @fileoverview Rule to disallow use of the new operator with global non-constructor functions
 * @author Sosuke Suzuki
 */

"use strict";

//------------------------------------------------------------------------------
// Helpers
//------------------------------------------------------------------------------

const nonConstructorGlobalFunctionNames = ["Symbol", "BigInt"];

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

/** @type {import('../shared/types').Rule} */
module.exports = {
    meta: {
        type: "problem",

        docs: {
            description: "Disallow `new` operators with global non-constructor functions",
            recommended: false,
            url: "https://eslint.org/docs/rules/no-new-native-nonconstructor"
        },

        schema: [],

        messages: {
            noNewNonconstructor: "`{{name}}` cannot be called as a constructor."
        }
    },

    create(context) {

        return {
            "Program:exit"() {
                const globalScope = context.getScope();

                for (const nonConstructorName of nonConstructorGlobalFunctionNames) {
                    const variable = globalScope.set.get(nonConstructorName);

                    if (variable && variable.defs.length === 0) {
                        variable.references.forEach(ref => {
                            const node = ref.identifier;
                            const parent = node.parent;

                            if (parent && parent.type === "NewExpression" && parent.callee === node) {
                                context.report({
                                    node,
                                    messageId: "noNewNonconstructor",
                                    data: { name: nonConstructorName }
                                });
                            }
                        });
                    }
                }
            }
        };

    }
};
