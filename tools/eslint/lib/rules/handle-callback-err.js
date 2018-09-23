/**
 * @fileoverview Ensure handling of errors when we know they exist.
 * @author Jamund Ferguson
 * @copyright 2015 Mathias Schreck.
 * @copyright 2014 Jamund Ferguson. All rights reserved.
 */

"use strict";

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = function(context) {

    var errorArgument = context.options[0] || "err";

    /**
     * Checks if the given argument should be interpreted as a regexp pattern.
     * @param {string} stringToCheck The string which should be checked.
     * @returns {boolean} Whether or not the string should be interpreted as a pattern.
     */
    function isPattern(stringToCheck) {
        var firstChar = stringToCheck[0];
        return firstChar === "^";
    }

    /**
     * Checks if the given name matches the configured error argument.
     * @param {string} name The name which should be compared.
     * @returns {boolean} Whether or not the given name matches the configured error variable name.
     */
    function matchesConfiguredErrorName(name) {
        if (isPattern(errorArgument)) {
            var regexp = new RegExp(errorArgument);
            return regexp.test(name);
        }
        return name === errorArgument;
    }

    /**
     * Get the parameters of a given function scope.
     * @param {object} scope The function scope.
     * @returns {array} All parameters of the given scope.
     */
    function getParameters(scope) {
        return scope.variables.filter(function (variable) {
            return variable.defs[0] && variable.defs[0].type === "Parameter";
        });
    }

    /**
     * Check to see if we're handling the error object properly.
     * @param {ASTNode} node The AST node to check.
     * @returns {void}
     */
    function checkForError(node) {
        var scope = context.getScope(),
            parameters = getParameters(scope),
            firstParameter = parameters[0];

        if (firstParameter && matchesConfiguredErrorName(firstParameter.name)) {
            if (firstParameter.references.length === 0) {
                context.report(node, "Expected error to be handled.");
            }
        }
    }

    return {
        "FunctionDeclaration": checkForError,
        "FunctionExpression": checkForError,
        "ArrowFunctionExpression": checkForError
    };

};

module.exports.schema = [
    {
        "type": "string"
    }
];
