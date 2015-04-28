/**
 * @fileoverview Rule to flag when the same variable is declared more then once.
 * @author Ilya Volodin
 */

"use strict";

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = function(context) {

    /**
     * Find variables in a given scope and flag redeclared ones.
     * @param {Scope} scope An escope scope object.
     * @returns {void}
     * @private
     */
    function findVariablesInScope(scope) {
        scope.variables.forEach(function(variable) {

            if (variable.identifiers && variable.identifiers.length > 1) {
                variable.identifiers.sort(function(a, b) {
                    return a.range[1] - b.range[1];
                });

                for (var i = 1, l = variable.identifiers.length; i < l; i++) {
                    context.report(variable.identifiers[i], "{{a}} is already defined", {a: variable.name});
                }
            }
        });

    }

    /**
     * Find variables in a given node's associated scope.
     * @param {ASTNode} node The node to check.
     * @returns {void}
     * @private
     */
    function findVariables(node) {
        var scope = context.getScope();

        findVariablesInScope(scope);

        // globalReturn means one extra scope to check
        if (node.type === "Program" && context.ecmaFeatures.globalReturn) {
            findVariablesInScope(scope.childScopes[0]);
        }
    }

    return {
        "Program": findVariables,
        "FunctionExpression": findVariables,
        "FunctionDeclaration": findVariables
    };
};
