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
            description: "enforce named `function` expressions",
            category: "Stylistic Issues",
            recommended: false
        },

        schema: []
    },

    create: function(context) {

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

                if (!name && !isObjectOrClassMethod()) {
                    context.report(node, "Missing function expression name.");
                }
            }
        };
    }
};
