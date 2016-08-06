/**
 * @fileoverview Rule to flag assignment of the exception parameter
 * @author Stephen Murray <spmurrayzzz>
 */

"use strict";

let astUtils = require("../ast-utils");

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = {
    meta: {
        docs: {
            description: "disallow reassigning exceptions in `catch` clauses",
            category: "Possible Errors",
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
                    "Do not assign to the exception parameter.");
            });
        }

        return {
            CatchClause: function(node) {
                context.getDeclaredVariables(node).forEach(checkVariable);
            }
        };

    }
};
