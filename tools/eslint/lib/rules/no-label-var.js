/**
 * @fileoverview Rule to flag labels that are the same as an identifier
 * @author Ian Christian Myers
 */

"use strict";

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = function(context) {

    //--------------------------------------------------------------------------
    // Helpers
    //--------------------------------------------------------------------------

    function findIdentifier(scope, identifier) {
        var found = false;

        scope.variables.forEach(function(variable) {
            if (variable.name === identifier) {
                found = true;
            }
        });

        scope.references.forEach(function(reference) {
            if (reference.identifier.name === identifier) {
                found = true;
            }
        });

        // If we have not found the identifier in this scope, check the parent
        // scope.
        if (scope.upper && !found) {
            return findIdentifier(scope.upper, identifier);
        }

        return found;
    }

    //--------------------------------------------------------------------------
    // Public API
    //--------------------------------------------------------------------------

    return {

        "LabeledStatement": function(node) {

            // Fetch the innermost scope.
            var scope = context.getScope();

            // Recursively find the identifier walking up the scope, starting
            // with the innermost scope.
            if (findIdentifier(scope, node.label.name)) {
                context.report(node, "Found identifier with same name as label.");
            }
        }

    };

};
