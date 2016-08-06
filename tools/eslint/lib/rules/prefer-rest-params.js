/**
 * @fileoverview Rule to
 * @author Toru Nagashima
 */

"use strict";

//------------------------------------------------------------------------------
// Helpers
//------------------------------------------------------------------------------

/**
 * Gets the variable object of `arguments` which is defined implicitly.
 * @param {escope.Scope} scope - A scope to get.
 * @returns {escope.Variable} The found variable object.
 */
function getVariableOfArguments(scope) {
    let variables = scope.variables;

    for (let i = 0; i < variables.length; ++i) {
        let variable = variables[i];

        if (variable.name === "arguments") {

            // If there was a parameter which is named "arguments", the implicit "arguments" is not defined.
            // So does fast return with null.
            return (variable.identifiers.length === 0) ? variable : null;
        }
    }

    /* istanbul ignore next : unreachable */
    return null;
}

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = {
    meta: {
        docs: {
            description: "require rest parameters instead of `arguments`",
            category: "ECMAScript 6",
            recommended: false
        },

        schema: []
    },

    create: function(context) {

        /**
         * Reports a given reference.
         *
         * @param {escope.Reference} reference - A reference to report.
         * @returns {void}
         */
        function report(reference) {
            context.report({
                node: reference.identifier,
                message: "Use the rest parameters instead of 'arguments'."
            });
        }

        /**
         * Reports references of the implicit `arguments` variable if exist.
         *
         * @returns {void}
         */
        function checkForArguments() {
            let argumentsVar = getVariableOfArguments(context.getScope());

            if (argumentsVar) {
                argumentsVar.references.forEach(report);
            }
        }

        return {
            FunctionDeclaration: checkForArguments,
            FunctionExpression: checkForArguments
        };
    }
};
