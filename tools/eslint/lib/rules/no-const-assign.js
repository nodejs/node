/**
 * @fileoverview A rule to disallow modifying variables that are declared using `const`
 * @author Toru Nagashima
 * @copyright 2015 Toru Nagashima. All rights reserved.
 */

"use strict";

var astUtils = require("../ast-utils");

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = function(context) {

    /**
     * Finds and reports references that are non initializer and writable.
     * @param {Variable} variable - A variable to check.
     * @returns {void}
     */
    function checkVariable(variable) {
        astUtils.getModifyingReferences(variable.references).forEach(function(reference) {
            context.report(
                reference.identifier,
                "`{{name}}` is constant.",
                {name: reference.identifier.name});
        });
    }

    return {
        "VariableDeclaration": function(node) {
            if (node.kind === "const") {
                context.getDeclaredVariables(node).forEach(checkVariable);
            }
        }
    };

};

module.exports.schema = [];
