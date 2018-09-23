/**
 * @fileoverview Rule to flag use of function declaration identifiers as variables.
 * @author Ian Christian Myers
 * @copyright 2013 Ian Christian Myers. All rights reserved.
 */

"use strict";

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = function(context) {

    //--------------------------------------------------------------------------
    // Helpers
    //--------------------------------------------------------------------------

    /*
     * Walk the scope chain looking for either a FunctionDeclaration or a
     * VariableDeclaration with the same name as left-hand side of the
     * AssignmentExpression. If we find the FunctionDeclaration first, then we
     * warn, because a FunctionDeclaration is trying to become a Variable or a
     * FunctionExpression. If we find a VariableDeclaration first, then we have
     * a legitimate shadow variable.
     */
    function checkIfIdentifierIsFunction(scope, name) {
        var variable,
            def,
            i,
            j;

        // Loop over all of the identifiers available in scope.
        for (i = 0; i < scope.variables.length; i++) {
            variable = scope.variables[i];

            // For each identifier, see if it was defined in _this_ scope.
            for (j = 0; j < variable.defs.length; j++) {
                def = variable.defs[j];

                // Identifier is a function and was declared in this scope
                if (def.type === "FunctionName" && def.name.name === name) {
                    return true;
                }

                // Identifier is a variable and was declared in this scope. This
                // is a legitimate shadow variable.
                if (def.name && def.name.name === name) {
                    return false;
                }
            }
        }

        // Check the upper scope.
        if (scope.upper) {
            return checkIfIdentifierIsFunction(scope.upper, name);
        }

        // We've reached the global scope and haven't found anything.
        return false;
    }

    //--------------------------------------------------------------------------
    // Public API
    //--------------------------------------------------------------------------

    return {

        "AssignmentExpression": function(node) {
            var scope = context.getScope(),
                name = node.left.name;

            if (checkIfIdentifierIsFunction(scope, name)) {
                context.report(node, "'{{name}}' is a function.", { name: name });
            }

        }

    };

};

module.exports.schema = [];
