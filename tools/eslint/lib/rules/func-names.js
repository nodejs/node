/**
 * @fileoverview Rule to warn when a function expression does not have a name.
 * @author Kyle T. Nunery
 */

"use strict";

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = {
    meta: {
        docs: {
            description: "require or disallow named `function` expressions",
            category: "Stylistic Issues",
            recommended: false
        },

        schema: [
            {
                enum: ["always", "never"]
            }
        ]
    },

    create: function(context) {
        var never = context.options[0] === "never";

        /**
         * Determines whether the current FunctionExpression node is a get, set, or
         * shorthand method in an object literal or a class.
         * @returns {boolean} True if the node is a get, set, or shorthand method.
         */
        function isObjectOrClassMethod() {
            var parent = context.getAncestors().pop();

            return (parent.type === "MethodDefinition" || (
                parent.type === "Property" && (
                    parent.method ||
                    parent.kind === "get" ||
                    parent.kind === "set"
                )
            ));
        }

        return {
            FunctionExpression: function(node) {

                var name = node.id && node.id.name;

                if (never) {
                    if (name) {
                        context.report(node, "Unexpected function expression name.");
                    }
                } else {
                    if (!name && !isObjectOrClassMethod()) {
                        context.report(node, "Missing function expression name.");
                    }
                }
            }
        };
    }
};
