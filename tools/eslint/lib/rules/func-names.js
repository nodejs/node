/**
 * @fileoverview Rule to warn when a function expression does not have a name.
 * @author Kyle T. Nunery
 */

"use strict";

/**
 * Checks whether or not a given variable is a function name.
 * @param {escope.Variable} variable - A variable to check.
 * @returns {boolean} `true` if the variable is a function name.
 */
function isFunctionName(variable) {
    return variable && variable.defs[0].type === "FunctionName";
}

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
        const never = context.options[0] === "never";

        /**
         * Determines whether the current FunctionExpression node is a get, set, or
         * shorthand method in an object literal or a class.
         * @returns {boolean} True if the node is a get, set, or shorthand method.
         */
        function isObjectOrClassMethod() {
            const parent = context.getAncestors().pop();

            return (parent.type === "MethodDefinition" || (
                parent.type === "Property" && (
                    parent.method ||
                    parent.kind === "get" ||
                    parent.kind === "set"
                )
            ));
        }

        return {
            "FunctionExpression:exit": function(node) {

                // Skip recursive functions.
                const nameVar = context.getDeclaredVariables(node)[0];

                if (isFunctionName(nameVar) && nameVar.references.length > 0) {
                    return;
                }

                const name = node.id && node.id.name;

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
