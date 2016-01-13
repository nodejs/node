/**
 * @fileoverview Rule to flag use of parseInt without a radix argument
 * @author James Allardice
 */

"use strict";

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = function(context) {

    var MODE_ALWAYS = "always",
        MODE_AS_NEEDED = "as-needed";

    var mode = context.options[0] || MODE_ALWAYS;

    return {
        "CallExpression": function(node) {

            var radix;

            if (!(node.callee.name === "parseInt" || (
                    node.callee.type === "MemberExpression" &&
                    node.callee.object.name === "Number" &&
                    node.callee.property.name === "parseInt"
                )
            )) {
                return;
            }

            if (node.arguments.length === 0) {
                context.report({
                    node: node,
                    message: "Missing parameters."
                });
            } else if (node.arguments.length < 2 && mode === MODE_ALWAYS) {
                context.report({
                    node: node,
                    message: "Missing radix parameter."
                });
            } else if (node.arguments.length > 1 && mode === MODE_AS_NEEDED &&
                (node.arguments[1] && node.arguments[1].type === "Literal" &&
                    node.arguments[1].value === 10)
            ) {
                context.report({
                    node: node,
                    message: "Redundant radix parameter."
                });
            } else {

                radix = node.arguments[1];

                // don't allow non-numeric literals or undefined
                if (radix &&
                    ((radix.type === "Literal" && typeof radix.value !== "number") ||
                    (radix.type === "Identifier" && radix.name === "undefined"))
                ) {
                    context.report({
                        node: node,
                        message: "Invalid radix parameter."
                    });
                }
            }

        }
    };

};

module.exports.schema = [
    {
        "enum": ["always", "as-needed"]
    }
];

