/**
 * @fileoverview Rule to flag variable leak in CatchClauses in IE 8 and earlier
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

    function paramIsShadowing(scope, name) {
        var found = scope.variables.some(function(variable) {
            return variable.name === name;
        });

        if (found) {
            return true;
        }

        if (scope.upper) {
            return paramIsShadowing(scope.upper, name);
        }

        return false;
    }

    //--------------------------------------------------------------------------
    // Public API
    //--------------------------------------------------------------------------

    return {

        "CatchClause": function(node) {
            var scope = context.getScope();

            if (paramIsShadowing(scope, node.param.name)) {
                context.report(node, "Value of '{{name}}' may be overwritten in IE 8 and earlier.",
                        { name: node.param.name });
            }
        }
    };

};

module.exports.schema = [];
