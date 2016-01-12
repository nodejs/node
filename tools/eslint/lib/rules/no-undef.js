/**
 * @fileoverview Rule to flag references to undeclared variables.
 * @author Mark Macdonald
 * @copyright 2015 Nicholas C. Zakas. All rights reserved.
 * @copyright 2013 Mark Macdonald. All rights reserved.
 */
"use strict";

//------------------------------------------------------------------------------
// Requirements
//------------------------------------------------------------------------------

var astUtils = require("../ast-utils");

//------------------------------------------------------------------------------
// Helpers
//------------------------------------------------------------------------------

var hasOwnProperty = Object.prototype.hasOwnProperty;

/**
 * Check if a variable is an implicit declaration
 * @param {ASTNode} variable node to evaluate
 * @returns {boolean} True if its an implicit declaration
 * @private
 */
function isImplicitGlobal(variable) {
    return variable.defs.every(function(def) {
        return def.type === "ImplicitGlobalVariable";
    });
}

/**
 * Gets the declared variable, defined in `scope`, that `ref` refers to.
 * @param {Scope} scope The scope in which to search.
 * @param {Reference} ref The reference to find in the scope.
 * @returns {Variable} The variable, or null if ref refers to an undeclared variable.
 */
function getDeclaredGlobalVariable(scope, ref) {
    var variable = astUtils.getVariableByName(scope, ref.identifier.name);

    // If it's an implicit global, it must have a `writeable` field (indicating it was declared)
    if (variable && (!isImplicitGlobal(variable) || hasOwnProperty.call(variable, "writeable"))) {
        return variable;
    }
    return null;
}

/**
 * Checks if the given node is the argument of a typeof operator.
 * @param {ASTNode} node The AST node being checked.
 * @returns {boolean} Whether or not the node is the argument of a typeof operator.
 */
function hasTypeOfOperator(node) {
    var parent = node.parent;
    return parent.type === "UnaryExpression" && parent.operator === "typeof";
}

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = function(context) {

    var NOT_DEFINED_MESSAGE = "\"{{name}}\" is not defined.",
        READ_ONLY_MESSAGE = "\"{{name}}\" is read only.";

    var options = context.options[0];
    var considerTypeOf = options && options.typeof === true || false;

    return {

        "Program:exit": function(/* node */) {

            var globalScope = context.getScope();

            globalScope.through.forEach(function(ref) {
                var variable = getDeclaredGlobalVariable(globalScope, ref),
                    name = ref.identifier.name;

                if (hasTypeOfOperator(ref.identifier) && !considerTypeOf) {
                    return;
                }

                if (!variable) {
                    context.report(ref.identifier, NOT_DEFINED_MESSAGE, { name: name });
                } else if (ref.isWrite() && variable.writeable === false) {
                    context.report(ref.identifier, READ_ONLY_MESSAGE, { name: name });
                }
            });

        }

    };

};

module.exports.schema = [
    {
        "type": "object",
        "properties": {
            "typeof": {
                "type": "boolean"
            }
        },
        "additionalProperties": false
    }
];
