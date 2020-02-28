/**
 * @fileoverview Rule to flag use of unary increment and decrement operators.
 * @author Ian Christian Myers
 * @author Brody McKee (github.com/mrmckeb)
 */

"use strict";

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = {
    meta: {
        type: "suggestion",

        docs: {
            description: "disallow the unary operators `++` and `--`",
            category: "Stylistic Issues",
            recommended: false,
            url: "https://eslint.org/docs/rules/no-plusplus"
        },

        schema: [
            {
                type: "object",
                properties: {
                    allowForLoopAfterthoughts: {
                        type: "boolean",
                        default: false
                    }
                },
                additionalProperties: false
            }
        ],

        messages: {
            unexpectedUnaryOp: "Unary operator '{{operator}}' used."
        }
    },

    create(context) {

        const config = context.options[0];
        let allowInForAfterthought = false;

        if (typeof config === "object") {
            allowInForAfterthought = config.allowForLoopAfterthoughts === true;
        }

        return {

            UpdateExpression(node) {
                if (allowInForAfterthought && node.parent.type === "ForStatement") {
                    return;
                }
                context.report({
                    node,
                    messageId: "unexpectedUnaryOp",
                    data: {
                        operator: node.operator
                    }
                });
            }

        };

    }
};
