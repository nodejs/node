/**
 * @fileoverview Disallow reassignment of function parameters.
 * @author Nat Burns
 * @copyright 2014 Nat Burns. All rights reserved.
 */
"use strict";

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = function(context) {

    //--------------------------------------------------------------------------
    // Helpers
    //--------------------------------------------------------------------------

    /**
     * Finds the declaration for a given variable by name, searching up the scope tree.
     * @param {Scope} scope The scope in which to search.
     * @param {String} name The name of the variable.
     * @returns {Variable} The declaration information for the given variable, or null if no declaration was found.
     */
    function findDeclaration(scope, name) {
        var variables = scope.variables;

        for (var i = 0; i < variables.length; i++) {
            if (variables[i].name === name) {
                return variables[i];
            }
        }

        if (scope.upper) {
            return findDeclaration(scope.upper, name);
        } else {
            return null;
        }
    }

    /**
     * Determines if a given variable is declared as a function parameter.
     * @param {Variable} variable The variable declaration.
     * @returns {boolean} True if the variable is a function parameter, false otherwise.
     */
    function isParameter(variable) {
        var defs = variable.defs;

        for (var i = 0; i < defs.length; i++) {
            if (defs[i].type === "Parameter") {
                return true;
            }
        }

        return false;
    }

    /**
     * Checks whether a given node is an assignment to a function parameter.
     * If so, a linting error will be reported.
     * @param {ASTNode} node The node to check.
     * @param {String} name The name of the variable being assigned to.
     * @returns {void}
     */
    function checkParameter(node, name) {
        var declaration = findDeclaration(context.getScope(), name);

        if (declaration && isParameter(declaration)) {
            context.report(node, "Assignment to function parameter '{{name}}'.", { name: name });
        }
    }

    //--------------------------------------------------------------------------
    // Public
    //--------------------------------------------------------------------------

    return {
        "AssignmentExpression": function(node) {
            checkParameter(node, node.left.name);
        },

        "UpdateExpression": function(node) {
            checkParameter(node, node.argument.name);
        }
    };
};
