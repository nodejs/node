/**
 * @fileoverview Rule to flag on declaring variables already declared in the outer scope
 * @author Ilya Volodin
 * @copyright 2013 Ilya Volodin. All rights reserved.
 */

"use strict";

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = function(context) {

    var options = {
        hoist: (context.options[0] && context.options[0].hoist) || "functions"
    };

    /**
     * Checks if a variable of the class name in the class scope of ClassDeclaration.
     *
     * ClassDeclaration creates two variables of its name into its outer scope and its class scope.
     * So we should ignore the variable in the class scope.
     *
     * @param {Object} variable The variable to check.
     * @returns {boolean} Whether or not the variable of the class name in the class scope of ClassDeclaration.
     */
    function isDuplicatedClassNameVariable(variable) {
        var block = variable.scope.block;
        return block.type === "ClassDeclaration" && block.id === variable.identifiers[0];
    }

    /**
     * Checks if a variable is inside the initializer of scopeVar.
     *
     * To avoid reporting at declarations such as `var a = function a() {};`.
     * But it should report `var a = function(a) {};` or `var a = function() { function a() {} };`.
     *
     * @param {Object} variable The variable to check.
     * @param {Object} scopeVar The scope variable to look for.
     * @returns {boolean} Whether or not the variable is inside initializer of scopeVar.
     */
    function isOnInitializer(variable, scopeVar) {
        var outerScope = scopeVar.scope;
        var outerDef = scopeVar.defs[0];
        var outer = outerDef && outerDef.parent && outerDef.parent.range;
        var innerScope = variable.scope;
        var innerDef = variable.defs[0];
        var inner = innerDef && innerDef.name.range;

        return (
            outer != null &&
            inner != null &&
            outer[0] < inner[0] &&
            inner[1] < outer[1] &&
            ((innerDef.type === "FunctionName" && innerDef.node.type === "FunctionExpression") || innerDef.node.type === "ClassExpression") &&
            outerScope === innerScope.upper
        );
    }

    /**
     * Get a range of a variable's identifier node.
     * @param {Object} variable The variable to get.
     * @returns {Array|undefined} The range of the variable's identifier node.
     */
    function getNameRange(variable) {
        var def = variable.defs[0];
        return def && def.name.range;
    }

    /**
     * Checks if a variable is in TDZ of scopeVar.
     * @param {Object} variable The variable to check.
     * @param {Object} scopeVar The variable of TDZ.
     * @returns {boolean} Whether or not the variable is in TDZ of scopeVar.
     */
    function isInTdz(variable, scopeVar) {
        var outerDef = scopeVar.defs[0];
        var inner = getNameRange(variable);
        var outer = getNameRange(scopeVar);
        return (
            inner != null &&
            outer != null &&
            inner[1] < outer[0] &&
            // Excepts FunctionDeclaration if is {"hoist":"function"}.
            (options.hoist !== "functions" || outerDef == null || outerDef.node.type !== "FunctionDeclaration")
        );
    }

    /**
     * Checks if a variable is contained in the list of given scope variables.
     * @param {Object} variable The variable to check.
     * @param {Array} scopeVars The scope variables to look for.
     * @returns {boolean} Whether or not the variable is contains in the list of scope variables.
     */
    function isContainedInScopeVars(variable, scopeVars) {
        return scopeVars.some(function (scopeVar) {
            return (
                scopeVar.identifiers.length > 0 &&
                variable.name === scopeVar.name &&
                !isDuplicatedClassNameVariable(scopeVar) &&
                !isOnInitializer(variable, scopeVar) &&
                !(options.hoist !== "all" && isInTdz(variable, scopeVar))
            );
        });
    }

    /**
     * Checks if the given variables are shadowed in the given scope.
     * @param {Array} variables The variables to look for
     * @param {Object} scope The scope to be checked.
     * @returns {Array} Variables which are not declared in the given scope.
     */
    function checkShadowsInScope(variables, scope) {

        var passedVars = [];

        variables.forEach(function (variable) {
            // "arguments" is a special case that has no identifiers (#1759)
            if (variable.identifiers.length > 0 && isContainedInScopeVars(variable, scope.variables)) {
                context.report(
                    variable.identifiers[0],
                    "{{name}} is already declared in the upper scope.",
                    {name: variable.name});
            } else {
                passedVars.push(variable);
            }
        });

        return passedVars;
    }

    /**
     * Checks the current context for shadowed variables.
     * @param {Scope} scope - Fixme
     * @returns {void}
     */
    function checkForShadows(scope) {
        var variables = scope.variables.filter(function(variable) {
            return (
                // Skip "arguments".
                variable.identifiers.length > 0 &&
                // Skip variables of a class name in the class scope of ClassDeclaration.
                !isDuplicatedClassNameVariable(variable)
            );
        });

        // iterate through the array of variables and find duplicates with the upper scope
        var upper = scope.upper;
        while (upper && variables.length) {
            variables = checkShadowsInScope(variables, upper);
            upper = upper.upper;
        }
    }

    return {
        "Program:exit": function () {
            var globalScope = context.getScope(),
                stack = globalScope.childScopes.slice(),
                scope;

            while (stack.length) {
                scope = stack.pop();
                stack.push.apply(stack, scope.childScopes);
                checkForShadows(scope);
            }
        }
    };

};

module.exports.schema = [
    {
        "type": "object",
        "properties": {
            "hoist": {
                "enum": ["all", "functions", "never"]
            }
        }
    }
];
