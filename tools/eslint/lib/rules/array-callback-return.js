/**
 * @fileoverview Rule to enforce return statements in callbacks of array's methods
 * @author Toru Nagashima
 */

"use strict";

//------------------------------------------------------------------------------
// Requirements
//------------------------------------------------------------------------------

let astUtils = require("../ast-utils");

//------------------------------------------------------------------------------
// Helpers
//------------------------------------------------------------------------------

let TARGET_NODE_TYPE = /^(?:Arrow)?FunctionExpression$/;
let TARGET_METHODS = /^(?:every|filter|find(?:Index)?|map|reduce(?:Right)?|some|sort)$/;

/**
 * Checks a given code path segment is reachable.
 *
 * @param {CodePathSegment} segment - A segment to check.
 * @returns {boolean} `true` if the segment is reachable.
 */
function isReachable(segment) {
    return segment.reachable;
}

/**
 * Gets a readable location.
 *
 * - FunctionExpression -> the function name or `function` keyword.
 * - ArrowFunctionExpression -> `=>` token.
 *
 * @param {ASTNode} node - A function node to get.
 * @param {SourceCode} sourceCode - A source code to get tokens.
 * @returns {ASTNode|Token} The node or the token of a location.
 */
function getLocation(node, sourceCode) {
    if (node.type === "ArrowFunctionExpression") {
        return sourceCode.getTokenBefore(node.body);
    }
    return node.id || node;
}

/**
 * Gets the name of a given node if the node is a Identifier node.
 *
 * @param {ASTNode} node - A node to get.
 * @returns {string} The name of the node, or an empty string.
 */
function getIdentifierName(node) {
    return node.type === "Identifier" ? node.name : "";
}

/**
 * Gets the value of a given node if the node is a Literal node or a
 * TemplateLiteral node.
 *
 * @param {ASTNode} node - A node to get.
 * @returns {string} The value of the node, or an empty string.
 */
function getConstantStringValue(node) {
    switch (node.type) {
        case "Literal":
            return String(node.value);

        case "TemplateLiteral":
            return node.expressions.length === 0
                ? node.quasis[0].value.cooked
                : "";

        default:
            return "";
    }
}

/**
 * Checks a given node is a MemberExpression node which has the specified name's
 * property.
 *
 * @param {ASTNode} node - A node to check.
 * @returns {boolean} `true` if the node is a MemberExpression node which has
 *      the specified name's property
 */
function isTargetMethod(node) {
    return (
        node.type === "MemberExpression" &&
        TARGET_METHODS.test(
            (node.computed ? getConstantStringValue : getIdentifierName)(node.property)
        )
    );
}

/**
 * Checks whether or not a given node is a function expression which is the
 * callback of an array method.
 *
 * @param {ASTNode} node - A node to check. This is one of
 *      FunctionExpression or ArrowFunctionExpression.
 * @returns {boolean} `true` if the node is the callback of an array method.
 */
function isCallbackOfArrayMethod(node) {
    while (node) {
        let parent = node.parent;

        switch (parent.type) {

            /*
             * Looks up the destination. e.g.,
             * foo.every(nativeFoo || function foo() { ... });
             */
            case "LogicalExpression":
            case "ConditionalExpression":
                node = parent;
                break;

            // If the upper function is IIFE, checks the destination of the return value.
            // e.g.
            //   foo.every((function() {
            //     // setup...
            //     return function callback() { ... };
            //   })());
            case "ReturnStatement": {
                const func = astUtils.getUpperFunction(parent);

                if (func === null || !astUtils.isCallee(func)) {
                    return false;
                }
                node = func.parent;
                break;
            }

            // e.g.
            //   Array.from([], function() {});
            //   list.every(function() {});
            case "CallExpression":
                if (astUtils.isArrayFromMethod(parent.callee)) {
                    return (
                        parent.arguments.length >= 2 &&
                        parent.arguments[1] === node
                    );
                }
                if (isTargetMethod(parent.callee)) {
                    return (
                        parent.arguments.length >= 1 &&
                        parent.arguments[0] === node
                    );
                }
                return false;

            // Otherwise this node is not target.
            default:
                return false;
        }
    }

    /* istanbul ignore next: unreachable */
    return false;
}

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = {
    meta: {
        docs: {
            description: "enforce `return` statements in callbacks of array methods",
            category: "Best Practices",
            recommended: false
        },

        schema: []
    },

    create: function(context) {
        let funcInfo = {
            upper: null,
            codePath: null,
            hasReturn: false,
            shouldCheck: false
        };

        /**
         * Checks whether or not the last code path segment is reachable.
         * Then reports this function if the segment is reachable.
         *
         * If the last code path segment is reachable, there are paths which are not
         * returned or thrown.
         *
         * @param {ASTNode} node - A node to check.
         * @returns {void}
         */
        function checkLastSegment(node) {
            if (funcInfo.shouldCheck &&
                funcInfo.codePath.currentSegments.some(isReachable)
            ) {
                context.report({
                    node: node,
                    loc: getLocation(node, context.getSourceCode()).loc.start,
                    message: funcInfo.hasReturn
                        ? "Expected to return a value at the end of this function."
                        : "Expected to return a value in this function."
                });
            }
        }

        return {

            // Stacks this function's information.
            onCodePathStart: function(codePath, node) {
                funcInfo = {
                    upper: funcInfo,
                    codePath: codePath,
                    hasReturn: false,
                    shouldCheck:
                        TARGET_NODE_TYPE.test(node.type) &&
                        node.body.type === "BlockStatement" &&
                        isCallbackOfArrayMethod(node)
                };
            },

            // Pops this function's information.
            onCodePathEnd: function() {
                funcInfo = funcInfo.upper;
            },

            // Checks the return statement is valid.
            ReturnStatement: function(node) {
                if (funcInfo.shouldCheck) {
                    funcInfo.hasReturn = true;

                    if (!node.argument) {
                        context.report({
                            node: node,
                            message: "Expected a return value."
                        });
                    }
                }
            },

            // Reports a given function if the last path is reachable.
            "FunctionExpression:exit": checkLastSegment,
            "ArrowFunctionExpression:exit": checkLastSegment
        };
    }
};
