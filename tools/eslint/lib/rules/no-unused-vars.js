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

    /**
     * Determines if a given variable is being exported from a module.
     * @param {Variable} variable EScope variable object.
     * @returns {boolean} True if the variable is exported, false if not.
     * @private
     */
    function isExported(variable) {

        var definition = variable.defs[0];

        if (definition) {

            definition = definition.node;
            if (definition.type === "VariableDeclarator") {
                definition = definition.parent;
            }

            return definition.parent.type.indexOf("Export") === 0;
        } else {
            return false;
        }
    }

    /**
     * Determines if a reference is a read operation.
     * @param {Reference} ref - an escope Reference
     * @returns {Boolean} whether the given reference represents a read operation
     * @private
     */
    function isReadRef(ref) {
        return ref.isRead();
    }

    /**
     * Determine if an identifier is referencing the enclosing function name.
     * @param {Reference} ref The reference to check.
     * @returns {boolean} True if it's a self-reference, false if not.
     * @private
     */
    function isSelfReference(ref) {

        if (ref.from.type === "function" && ref.from.block.id) {
            return ref.identifier.name === ref.from.block.id.name;
        }

        return false;
    }

    /**
     * Determines if a reference should be counted as a read. A reference should
     * be counted only if it's a read and it's not a reference to the containing
     * function declaration name.
     * @param {Reference} ref The reference to check.
     * @returns {boolean} True if it's a value read reference, false if not.
     * @private
     */
    function isValidReadRef(ref) {
        return isReadRef(ref) && !isSelfReference(ref);
    }

    /**
     * Gets an array of local variables without read references.
     * @param {Scope} scope - an escope Scope object
     * @returns {Variable[]} most of the local variables with no read references
     * @private
     */
    function getUnusedLocals(scope) {
        var unused = [];
        var variables = scope.variables;

        if (scope.type !== "global" && scope.type !== "TDZ") {
            for (var i = 0, l = variables.length; i < l; ++i) {

                // skip function expression names
                if (scope.functionExpressionScope || variables[i].eslintUsed) {
                    continue;
                }
                // skip implicit "arguments" variable
                if (scope.type === "function" && variables[i].name === "arguments" && variables[i].identifiers.length === 0) {
                    continue;
                }

                var def = variables[i].defs[0],
                    type = def.type;

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
                if (config.args === "after-used" && type === "Parameter" && variables[i].defs[0].index < variables[i].defs[0].node.params.length - 1) {
                    continue;
                }

                if (variables[i].references.filter(isValidReadRef).length === 0 && !isExported(variables[i])) {
                    unused.push(variables[i]);
                }
            }
        }

        return [].concat.apply(unused, scope.childScopes.map(getUnusedLocals));
    }

    return {
        "Program:exit": function(programNode) {
            var globalScope = context.getScope();
            var unused = getUnusedLocals(globalScope);
            var i, l;

            // determine unused globals
            if (config.vars === "all") {
                var unresolvedRefs = globalScope.through.filter(isValidReadRef).map(function(ref) {
                    return ref.identifier.name;
                });

                for (i = 0, l = globalScope.variables.length; i < l; ++i) {
                    if (unresolvedRefs.indexOf(globalScope.variables[i].name) < 0 &&
                            !globalScope.variables[i].eslintUsed && !isExported(globalScope.variables[i])) {
                        unused.push(globalScope.variables[i]);
                    }
                }
            }

            for (i = 0, l = unused.length; i < l; ++i) {
                if (unused[i].eslintExplicitGlobal) {
                    context.report(programNode, MESSAGE, unused[i]);
                } else if (unused[i].defs.length > 0) {

                    // TODO: Remove when https://github.com/estools/escope/issues/49 is resolved
                    if (unused[i].defs[0].type === "ClassName") {
                        continue;
                    }

                    context.report(unused[i].identifiers[0], MESSAGE, unused[i]);
                }
            }
        }
    };

};
