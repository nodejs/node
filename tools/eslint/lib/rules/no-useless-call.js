/**
 * @fileoverview A rule to disallow unnecessary `.call()` and `.apply()`.
 * @author Toru Nagashima
 */

"use strict";

var astUtils = require("../ast-utils");

//------------------------------------------------------------------------------
// Helpers
//------------------------------------------------------------------------------

/**
 * Checks whether or not a node is a `.call()`/`.apply()`.
 * @param {ASTNode} node - A CallExpression node to check.
 * @returns {boolean} Whether or not the node is a `.call()`/`.apply()`.
 */
function isCallOrNonVariadicApply(node) {
    return (
        node.callee.type === "MemberExpression" &&
        node.callee.property.type === "Identifier" &&
        node.callee.computed === false &&
        (
            (node.callee.property.name === "call" && node.arguments.length >= 1) ||
            (node.callee.property.name === "apply" && node.arguments.length === 2 && node.arguments[1].type === "ArrayExpression")
        )
    );
}

/**
 * Checks whether or not the tokens of two given nodes are same.
 * @param {ASTNode} left - A node 1 to compare.
 * @param {ASTNode} right - A node 2 to compare.
 * @param {RuleContext} context - The ESLint rule context object.
 * @returns {boolean} the source code for the given node.
 */
function equalTokens(left, right, context) {
    var tokensL = context.getTokens(left);
    var tokensR = context.getTokens(right);

    if (tokensL.length !== tokensR.length) {
        return false;
    }
    for (var i = 0; i < tokensL.length; ++i) {
        if (tokensL[i].type !== tokensR[i].type ||
            tokensL[i].value !== tokensR[i].value
        ) {
            return false;
        }
    }

    return true;
}

/**
 * Checks whether or not `thisArg` is not changed by `.call()`/`.apply()`.
 * @param {ASTNode|null} expectedThis - The node that is the owner of the applied function.
 * @param {ASTNode} thisArg - The node that is given to the first argument of the `.call()`/`.apply()`.
 * @param {RuleContext} context - The ESLint rule context object.
 * @returns {boolean} Whether or not `thisArg` is not changed by `.call()`/`.apply()`.
 */
function isValidThisArg(expectedThis, thisArg, context) {
    if (!expectedThis) {
        return astUtils.isNullOrUndefined(thisArg);
    }
    return equalTokens(expectedThis, thisArg, context);
}

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = {
    meta: {
        docs: {
            description: "disallow unnecessary calls to `.call()` and `.apply()`",
            category: "Best Practices",
            recommended: false
        },

        schema: []
    },

    create: function(context) {
        return {
            CallExpression: function(node) {
                if (!isCallOrNonVariadicApply(node)) {
                    return;
                }

                var applied = node.callee.object;
                var expectedThis = (applied.type === "MemberExpression") ? applied.object : null;
                var thisArg = node.arguments[0];

                if (isValidThisArg(expectedThis, thisArg, context)) {
                    context.report(
                        node,
                        "unnecessary '.{{name}}()'.",
                        {name: node.callee.property.name});
                }
            }
        };
    }
};
