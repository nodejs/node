/**
 * @fileoverview Rule to flag use of variables before they are defined
 * @author Ilya Volodin
 * @copyright 2013 Ilya Volodin. All rights reserved.
 */

"use strict";

//------------------------------------------------------------------------------
// Constants
//------------------------------------------------------------------------------

var NO_FUNC = "nofunc";

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = function(context) {

    /**
     * Finds variable declarations in a given scope.
     * @param {string} name The variable name to find.
     * @param {Scope} scope The scope to search in.
     * @returns {Object} The variable declaration object.
     * @private
     */
    function findDeclaration(name, scope) {
        // try searching in the current scope first
        for (var i = 0, l = scope.variables.length; i < l; i++) {
            if (scope.variables[i].name === name) {
                return scope.variables[i];
            }
        }
        // check if there's upper scope and call recursivly till we find the variable
        if (scope.upper) {
            return findDeclaration(name, scope.upper);
        }
    }

    /**
     * Finds and validates all variables in a given scope.
     * @param {Scope} scope The scope object.
     * @returns {void}
     * @private
     */
    function findVariablesInScope(scope) {
        var typeOption = context.options[0];

        function checkLocationAndReport(reference, declaration) {
            if (typeOption !== NO_FUNC || declaration.defs[0].type !== "FunctionName") {
                if (declaration.identifiers[0].range[1] > reference.identifier.range[1]) {
                    context.report(reference.identifier, "{{a}} was used before it was defined", {a: reference.identifier.name});
                }
            }
        }

        scope.references.forEach(function(reference) {
            // if the reference is resolved check for declaration location
            // if not, it could be function invocation, try to find manually
            if (reference.resolved && reference.resolved.identifiers.length > 0) {
                checkLocationAndReport(reference, reference.resolved);
            } else {
                var declaration = findDeclaration(reference.identifier.name, scope);
                // if there're no identifiers, this is a global environment variable
                if (declaration && declaration.identifiers.length !== 0) {
                    checkLocationAndReport(reference, declaration);
                }
            }
        });
    }


    /**
     * Validates variables inside of a node's scope.
     * @param {ASTNode} node The node to check.
     * @returns {void}
     * @private
     */
    function findVariables() {
        var scope = context.getScope();
        findVariablesInScope(scope);
    }

    return {
        "Program": function() {
            var scope = context.getScope();
            findVariablesInScope(scope);

            // both Node.js and Modules have an extra scope
            if (context.ecmaFeatures.globalReturn || context.ecmaFeatures.modules) {
                findVariablesInScope(scope.childScopes[0]);
            }
        },
        "FunctionExpression": findVariables,
        "FunctionDeclaration": findVariables,
        "ArrowFunctionExpression": findVariables
    };
};

module.exports.schema = [
    {
        "enum": ["nofunc"]
    }
];
