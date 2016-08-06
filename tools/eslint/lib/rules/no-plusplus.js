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
        docs: {
            description: "disallow the unary operators `++` and `--`",
            category: "Stylistic Issues",
            recommended: false
        },

        schema: [
            {
                type: "object",
                properties: {
                    allowForLoopAfterthoughts: {
                        type: "boolean"
                    }
                },
                additionalProperties: false
            }
        ]
    },

    create: function(context) {

        let config = context.options[0],
            allowInForAfterthought = false;

        if (typeof config === "object") {
            allowInForAfterthought = config.allowForLoopAfterthoughts === true;
        }

        return {

            UpdateExpression: function(node) {
                if (allowInForAfterthought && node.parent.type === "ForStatement") {
                    return;
                }
                context.report(node, "Unary operator '" + node.operator + "' used.");
            }

        };

    }
};
