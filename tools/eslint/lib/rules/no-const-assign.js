/**
 * @fileoverview A rule to disallow modifying variables that are declared using `const`
 * @author Toru Nagashima
 */

"use strict";

const astUtils = require("../ast-utils");

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = {
    meta: {
        docs: {
            description: "disallow reassigning `const` variables",
            category: "ECMAScript 6",
            recommended: true
        },

        schema: []
    },

    create: function(context) {

        /**
         * Finds and reports references that are non initializer and writable.
         * @param {Variable} variable - A variable to check.
         * @returns {void}
         */
        function checkVariable(variable) {
            astUtils.getModifyingReferences(variable.references).forEach(function(reference) {
                context.report(
                    reference.identifier,
                    "'{{name}}' is constant.",
                    {name: reference.identifier.name});
            });
        }

        return {
            VariableDeclaration: function(node) {
                if (node.kind === "const") {
                    context.getDeclaredVariables(node).forEach(checkVariable);
                }
            }
        };

    }
};
