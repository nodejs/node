/**
 * @fileoverview Rule to flag declared but unused variables
 * @author Ilya Volodin
 */

"use strict";

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = function(context) {

    var MESSAGE = "{{name}} is defined but never used";

    var config = {
        vars: "all",
        args: "after-used"
    };

    if (context.options[0]) {
        if (typeof context.options[0] === "string") {
            config.vars = context.options[0];
        } else {
            config.vars = context.options[0].vars || config.vars;
            config.args = context.options[0].args || config.args;
        }
    }

    //--------------------------------------------------------------------------
    // Helpers
    //--------------------------------------------------------------------------


    /**
     * Determines if a given variable is being exported from a module.
     * @param {Variable} variable - EScope variable object.
     * @returns {boolean} True if the variable is exported, false if not.
     * @private
     */
    function isExported(variable) {

        var definition = variable.defs[0];

        if (definition) {

            var node = definition.node;
            if (node.type === "VariableDeclarator") {
                node = node.parent;
            } else if (definition.type === "Parameter" && node.type === "FunctionDeclaration") {
                return false;
            }

            return node.parent.type.indexOf("Export") === 0;
        } else {
            return false;
        }
    }

    /**
     * Determines if a reference is a read operation.
     * @param {Reference} ref - An escope Reference
     * @returns {Boolean} whether the given reference represents a read operation
     * @private
     */
    function isReadRef(ref) {
        return ref.isRead();
    }

    /**
     * Determine if an identifier is referencing an enclosing function name.
     * @param {Reference} ref - The reference to check.
     * @param {ASTNode[]} nodes - The candidate function nodes.
     * @returns {boolean} True if it's a self-reference, false if not.
     * @private
     */
    function isSelfReference(ref, nodes) {
        var scope = ref.from;

        while (scope != null) {
            if (nodes.indexOf(scope.block) >= 0) {
                return true;
            }

            scope = scope.upper;
        }

        return false;
    }

    /**
     * Determines if the variable is used.
     * @param {Variable} variable - The variable to check.
     * @param {Reference[]} references - The variable references to check.
     * @returns {boolean} True if the variable is used
     */
    function isUsedVariable(variable, references) {
        var functionNodes = variable.defs.filter(function (def) {
            return def.type === "FunctionName";
        }).map(function (def) {
            return def.node;
        }),
            isFunctionDefinition = functionNodes.length > 0;

        return references.some(function (ref) {
            return isReadRef(ref) && !(isFunctionDefinition && isSelfReference(ref, functionNodes));
        });
    }

    /**
     * Gets unresolved references.
     * They contains var's, function's, and explicit global variable's.
     * If `config.vars` is not "all", returns empty map.
     * @param {Scope} scope - the global scope.
     * @returns {object} Unresolved references. Keys of the object is its variable name. Values of the object is an array of its references.
     * @private
     */
    function collectUnresolvedReferences(scope) {
        var unresolvedRefs = Object.create(null);

        if (config.vars === "all") {
            for (var i = 0, l = scope.through.length; i < l; ++i) {
                var ref = scope.through[i];
                var name = ref.identifier.name;

                if (isReadRef(ref)) {
                    if (!unresolvedRefs[name]) {
                        unresolvedRefs[name] = [];
                    }
                    unresolvedRefs[name].push(ref);
                }
            }
        }

        return unresolvedRefs;
    }

    /**
     * Gets an array of variables without read references.
     * @param {Scope} scope - an escope Scope object.
     * @param {object} unresolvedRefs - a map of each variable name and its references.
     * @param {Variable[]} unusedVars - an array that saving result.
     * @returns {Variable[]} unused variables of the scope and descendant scopes.
     * @private
     */
    function collectUnusedVariables(scope, unresolvedRefs, unusedVars) {
        var variables = scope.variables;
        var childScopes = scope.childScopes;
        var i, l;

        if (scope.type !== "TDZ" && (scope.type !== "global" || config.vars === "all")) {
            for (i = 0, l = variables.length; i < l; ++i) {
                var variable = variables[i];

                // skip a variable of class itself name in the class scope
                if (scope.type === "class" && scope.block.id === variable.identifiers[0]) {
                    continue;
                }
                // skip function expression names
                if (scope.functionExpressionScope || variable.eslintUsed) {
                    continue;
                }
                // skip implicit "arguments" variable
                if (scope.type === "function" && variable.name === "arguments" && variable.identifiers.length === 0) {
                    continue;
                }

                // explicit global variables don't have definitions.
                var def = variable.defs[0];
                if (def != null) {
                    var type = def.type;

                    // skip catch variables
                    if (type === "CatchClause") {
                        continue;
                    }

                    // skip any setter argument
                    if (type === "Parameter" && def.node.parent.type === "Property" && def.node.parent.kind === "set") {
                        continue;
                    }

                    // if "args" option is "none", skip any parameter
                    if (config.args === "none" && type === "Parameter") {
                        continue;
                    }

                    // if "args" option is "after-used", skip all but the last parameter
                    if (config.args === "after-used" && type === "Parameter" && def.index < def.node.params.length - 1) {
                        continue;
                    }
                }

                // On global, variables without let/const/class are unresolved.
                var references = (scope.type === "global" ? unresolvedRefs[variable.name] : null) || variable.references;
                if (!isUsedVariable(variable, references) && !isExported(variable)) {
                    unusedVars.push(variable);
                }
            }
        }

        for (i = 0, l = childScopes.length; i < l; ++i) {
            collectUnusedVariables(childScopes[i], unresolvedRefs, unusedVars);
        }

        return unusedVars;
    }

    //--------------------------------------------------------------------------
    // Public
    //--------------------------------------------------------------------------

    return {
        "Program:exit": function(programNode) {
            var globalScope = context.getScope();
            var unresolvedRefs = collectUnresolvedReferences(globalScope);
            var unusedVars = collectUnusedVariables(globalScope, unresolvedRefs, []);

            for (var i = 0, l = unusedVars.length; i < l; ++i) {
                var unusedVar = unusedVars[i];

                if (unusedVar.eslintExplicitGlobal) {
                    context.report(programNode, MESSAGE, unusedVar);
                } else if (unusedVar.defs.length > 0) {
                    context.report(unusedVar.identifiers[0], MESSAGE, unusedVar);
                }
            }
        }
    };

};

module.exports.schema = [
    {
        "oneOf": [
            {
                "enum": ["all", "local"]
            },
            {
                "type": "object",
                "properties": {
                    "vars": {
                        "enum": ["all", "local"]
                    },
                    "args": {
                        "enum": ["all", "after-used", "none"]
                    }
                }
            }
        ]
    }
];
