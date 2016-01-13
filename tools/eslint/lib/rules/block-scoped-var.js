/**
 * @fileoverview Rule to check for "block scoped" variables by binding context
 * @author Matt DuVall <http://www.mattduvall.com>
 * @copyright 2015 Toru Nagashima. All rights reserved.
 * @copyright 2015 Mathieu M-Gosselin. All rights reserved.
 */
"use strict";

//------------------------------------------------------------------------------
// Helpers
//------------------------------------------------------------------------------

/**
 * Collects unresolved references from the global scope, then creates a map to references from its name.
 * @param {RuleContext} context - The current context.
 * @returns {object} A map object. Its key is the variable names. Its value is the references of each variable.
 */
function collectUnresolvedReferences(context) {
    var unresolved = Object.create(null);
    var references = context.getScope().through;

    for (var i = 0; i < references.length; ++i) {
        var reference = references[i];
        var name = reference.identifier.name;

        if (name in unresolved === false) {
            unresolved[name] = [];
        }
        unresolved[name].push(reference);
    }

    return unresolved;
}

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = function(context) {
    var unresolvedReferences = Object.create(null);
    var stack = [];

    /**
     * Makes a block scope.
     * @param {ASTNode} node - A node of a scope.
     * @returns {void}
     */
    function enterScope(node) {
        stack.push(node.range);
    }

    /**
     * Pops the last block scope.
     * @returns {void}
     */
    function exitScope() {
        stack.pop();
    }

    /**
     * Reports a given reference.
     * @param {escope.Reference} reference - A reference to report.
     * @returns {void}
     */
    function report(reference) {
        var identifier = reference.identifier;
        context.report(
            identifier,
            "\"{{name}}\" used outside of binding context.",
            {name: identifier.name});
    }

    /**
     * Finds and reports references which are outside of valid scopes.
     * @param {ASTNode} node - A node to get variables.
     * @returns {void}
     */
    function checkForVariables(node) {
        if (node.kind !== "var") {
            return;
        }

        var isGlobal = context.getScope().type === "global";

        // Defines a predicate to check whether or not a given reference is outside of valid scope.
        var scopeRange = stack[stack.length - 1];

        /**
         * Check if a reference is out of scope
         * @param {ASTNode} reference node to examine
         * @returns {boolean} True is its outside the scope
         * @private
         */
        function isOutsideOfScope(reference) {
            var idRange = reference.identifier.range;
            return idRange[0] < scopeRange[0] || idRange[1] > scopeRange[1];
        }

        // Gets declared variables, and checks its references.
        var variables = context.getDeclaredVariables(node);
        for (var i = 0; i < variables.length; ++i) {
            var variable = variables[i];
            var references = variable.references;

            // Global variables are not resolved.
            // In this case, use unresolved references.
            if (isGlobal && variable.name in unresolvedReferences) {
                references = unresolvedReferences[variable.name];
            }

            // Reports.
            references.filter(isOutsideOfScope).forEach(report);
        }
    }

    return {
        "Program": function(node) {
            unresolvedReferences = collectUnresolvedReferences(context);
            stack = [node.range];
        },

        // Manages scopes.
        "BlockStatement": enterScope,
        "BlockStatement:exit": exitScope,
        "ForStatement": enterScope,
        "ForStatement:exit": exitScope,
        "ForInStatement": enterScope,
        "ForInStatement:exit": exitScope,
        "ForOfStatement": enterScope,
        "ForOfStatement:exit": exitScope,
        "SwitchStatement": enterScope,
        "SwitchStatement:exit": exitScope,
        "CatchClause": enterScope,
        "CatchClause:exit": exitScope,

        // Finds and reports references which are outside of valid scope.
        "VariableDeclaration": checkForVariables
    };

};

module.exports.schema = [];
