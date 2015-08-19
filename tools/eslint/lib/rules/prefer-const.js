/**
 * @fileoverview A rule to suggest using of const declaration for variables that are never modified after declared.
 * @author Toru Nagashima
 * @copyright 2015 Toru Nagashima. All rights reserved.
 */

"use strict";

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = function(context) {

    /**
     * Checks whether a reference is the initializer.
     * @param {Reference} reference - A reference to check.
     * @returns {boolean} Whether or not the reference is the initializer.
     */
    function isInitializer(reference) {
        return reference.init === true;
    }

    /**
     * Checks whether a reference is read-only or the initializer.
     * @param {Reference} reference - A reference to check.
     * @returns {boolean} Whether or not the reference is read-only or the initializer.
     */
    function isReadOnlyOrInitializer(reference) {
        return reference.isReadOnly() || reference.init === true;
    }

    /**
     * Searches and reports variables that are never modified after declared.
     * @param {Scope} scope - A scope of the search domain.
     * @returns {void}
     */
    function checkForVariables(scope) {
        // Skip the TDZ type.
        if (scope.type === "TDZ") {
            return;
        }

        var variables = scope.variables;
        for (var i = 0, end = variables.length; i < end; ++i) {
            var variable = variables[i];
            var def = variable.defs[0];
            var declaration = def && def.parent;
            var statement = declaration && declaration.parent;
            var references = variable.references;
            var identifier = variable.identifiers[0];

            if (statement != null &&
                identifier != null &&
                declaration.type === "VariableDeclaration" &&
                declaration.kind === "let" &&
                (statement.type !== "ForStatement" || statement.init !== declaration) &&
                references.some(isInitializer) &&
                references.every(isReadOnlyOrInitializer)
            ) {
                context.report(
                    identifier,
                    "`{{name}}` is never modified, use `const` instead.",
                    {name: identifier.name});
            }
        }
    }

    /**
     * Adds multiple items to the tail of an array.
     * @param {any[]} array - A destination to add.
     * @param {any[]} values - Items to be added.
     * @returns {void}
     */
    var pushAll = Function.apply.bind(Array.prototype.push);

    return {
        "Program:exit": function () {
            var stack = [context.getScope()];
            while (stack.length) {
                var scope = stack.pop();
                pushAll(stack, scope.childScopes);

                checkForVariables(scope);
            }
        }
    };

};

module.exports.schema = [];
