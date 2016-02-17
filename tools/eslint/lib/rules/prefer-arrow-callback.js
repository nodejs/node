/**
 * @fileoverview A rule to suggest using arrow functions as callbacks.
 * @author Toru Nagashima
 * @copyright 2015 Toru Nagashima. All rights reserved.
 */

"use strict";

//------------------------------------------------------------------------------
// Helpers
//------------------------------------------------------------------------------

/**
 * Checks whether or not a given variable is a function name.
 * @param {escope.Variable} variable - A variable to check.
 * @returns {boolean} `true` if the variable is a function name.
 */
function isFunctionName(variable) {
    return variable && variable.defs[0].type === "FunctionName";
}

/**
 * Checks whether or not a given MetaProperty node equals to a given value.
 * @param {ASTNode} node - A MetaProperty node to check.
 * @param {string} metaName - The name of `MetaProperty.meta`.
 * @param {string} propertyName - The name of `MetaProperty.property`.
 * @returns {boolean} `true` if the node is the specific value.
 */
function checkMetaProperty(node, metaName, propertyName) {
    // TODO: Remove this if block after https://github.com/eslint/espree/issues/206 was fixed.
    if (typeof node.meta === "string") {
        return node.meta === metaName && node.property === propertyName;
    }
    return node.meta.name === metaName && node.property.name === propertyName;
}

/**
 * Gets the variable object of `arguments` which is defined implicitly.
 * @param {escope.Scope} scope - A scope to get.
 * @returns {escope.Variable} The found variable object.
 */
function getVariableOfArguments(scope) {
    var variables = scope.variables;
    for (var i = 0; i < variables.length; ++i) {
        var variable = variables[i];
        if (variable.name === "arguments") {
            // If there was a parameter which is named "arguments", the implicit "arguments" is not defined.
            // So does fast return with null.
            return (variable.identifiers.length === 0) ? variable : null;
        }
    }

    /* istanbul ignore next */
    return null;
}

/**
 * Checkes whether or not a given node is a callback.
 * @param {ASTNode} node - A node to check.
 * @returns {object}
 *   {boolean} retv.isCallback - `true` if the node is a callback.
 *   {boolean} retv.isLexicalThis - `true` if the node is with `.bind(this)`.
 */
function getCallbackInfo(node) {
    var retv = {isCallback: false, isLexicalThis: false};
    var parent = node.parent;
    while (node) {
        switch (parent.type) {
            // Checks parents recursively.
            case "LogicalExpression":
            case "ConditionalExpression":
                break;

            // Checks whether the parent node is `.bind(this)` call.
            case "MemberExpression":
                if (parent.object === node &&
                    !parent.property.computed &&
                    parent.property.type === "Identifier" &&
                    parent.property.name === "bind" &&
                    parent.parent.type === "CallExpression" &&
                    parent.parent.callee === parent
                ) {
                    retv.isLexicalThis = (
                        parent.parent.arguments.length === 1 &&
                        parent.parent.arguments[0].type === "ThisExpression"
                    );
                    node = parent;
                    parent = parent.parent;
                } else {
                    return retv;
                }
                break;

            // Checks whether the node is a callback.
            case "CallExpression":
            case "NewExpression":
                if (parent.callee !== node) {
                    retv.isCallback = true;
                }
                return retv;

            default:
                return retv;
        }

        node = parent;
        parent = parent.parent;
    }

    /* istanbul ignore next */
    throw new Error("unreachable");
}

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = function(context) {
    // {Array<{this: boolean, super: boolean, meta: boolean}>}
    // - this - A flag which shows there are one or more ThisExpression.
    // - super - A flag which shows there are one or more Super.
    // - meta - A flag which shows there are one or more MethProperty.
    var stack = [];

    /**
     * Pushes new function scope with all `false` flags.
     * @returns {void}
     */
    function enterScope() {
        stack.push({this: false, super: false, meta: false});
    }

    /**
     * Pops a function scope from the stack.
     * @returns {{this: boolean, super: boolean, meta: boolean}} The information of the last scope.
     */
    function exitScope() {
        return stack.pop();
    }

    return {
        // Reset internal state.
        Program: function() {
            stack = [];
        },

        // If there are below, it cannot replace with arrow functions merely.
        ThisExpression: function() {
            var info = stack[stack.length - 1];
            if (info) {
                info.this = true;
            }
        },
        Super: function() {
            var info = stack[stack.length - 1];
            if (info) {
                info.super = true;
            }
        },
        MetaProperty: function(node) {
            var info = stack[stack.length - 1];
            if (info && checkMetaProperty(node, "new", "target")) {
                info.meta = true;
            }
        },

        // To skip nested scopes.
        FunctionDeclaration: enterScope,
        "FunctionDeclaration:exit": exitScope,

        // Main.
        FunctionExpression: enterScope,
        "FunctionExpression:exit": function(node) {
            var scopeInfo = exitScope();

            // Skip generators.
            if (node.generator) {
                return;
            }

            // Skip recursive functions.
            var nameVar = context.getDeclaredVariables(node)[0];
            if (isFunctionName(nameVar) && nameVar.references.length > 0) {
                return;
            }

            // Skip if it's using arguments.
            var variable = getVariableOfArguments(context.getScope());
            if (variable && variable.references.length > 0) {
                return;
            }

            // Reports if it's a callback which can replace with arrows.
            var callbackInfo = getCallbackInfo(node);
            if (callbackInfo.isCallback &&
                (!scopeInfo.this || callbackInfo.isLexicalThis) &&
                !scopeInfo.super &&
                !scopeInfo.meta
            ) {
                context.report(node, "Unexpected function expression.");
            }
        }
    };
};

module.exports.schema = [];
