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

        schema: [],

        fixable: "code"
    },
    create(context) {
        const sourceCode = context.getSourceCode();

        return {
            Property(node) {
                if (!node.computed) {
                    return;
                }

                const key = node.key,
                    nodeType = typeof key.value;

                if (key.type === "Literal" && (nodeType === "string" || nodeType === "number")) {
                    context.report({
                        node,
                        message: MESSAGE_UNNECESSARY_COMPUTED,
                        data: { property: sourceCode.getText(key) },
                        fix(fixer) {
                            const leftSquareBracket = sourceCode.getFirstToken(node, node.value.generator || node.value.async ? 1 : 0);
                            const rightSquareBracket = sourceCode.getTokensBetween(node.key, node.value).find(token => token.value === "]");

                            const tokensBetween = sourceCode.getTokensBetween(leftSquareBracket, rightSquareBracket, 1);

                            if (tokensBetween.slice(0, -1).some((token, index) => sourceCode.getText().slice(token.range[1], tokensBetween[index + 1].range[0]).trim())) {

                                // If there are comments between the brackets and the property name, don't do a fix.
                                return null;
                            }
                            return fixer.replaceTextRange([leftSquareBracket.range[0], rightSquareBracket.range[1]], key.raw);
                        }
                    });
                }
            }
        };
    }
};
