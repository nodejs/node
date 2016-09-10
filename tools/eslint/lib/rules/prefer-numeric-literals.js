/**
 * @fileoverview Rule to disallow `parseInt()` in favor of binary, octal, and hexadecimal literals
 * @author Annie Zhang, Henry Zhu
 */

"use strict";

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = {
    meta: {
        docs: {
            description: "disallow `parseInt()` in favor of binary, octal, and hexadecimal literals",
            category: "ECMAScript 6",
            recommended: false
        },

        schema: []
    },

    create(context) {
        const radixMap = {
            2: "binary",
            8: "octal",
            16: "hexadecimal"
        };

        //--------------------------------------------------------------------------
        // Public
        //--------------------------------------------------------------------------

        return {

            CallExpression(node) {

                // doesn't check parseInt() if it doesn't have a radix argument
                if (node.arguments.length !== 2) {
                    return;
                }

                // only error if the radix is 2, 8, or 16
                const radixName = radixMap[node.arguments[1].value];

                if (node.callee.type === "Identifier" &&
                    node.callee.name === "parseInt" &&
                    radixName &&
                    node.arguments[0].type === "Literal"
                ) {
                    context.report({
                        node,
                        message: "Use {{radixName}} literals instead of parseInt().",
                        data: {
                            radixName
                        }
                    });
                }
            }
        };
    }
};
