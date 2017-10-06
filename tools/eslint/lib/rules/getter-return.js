/**
 * @fileoverview Enforces that a return statement is present in property getters.
 * @author Aladdin-ADD(hh_2013@foxmail.com)
 */

"use strict";

//------------------------------------------------------------------------------
// Requirements
//------------------------------------------------------------------------------

const astUtils = require("../ast-utils");

//------------------------------------------------------------------------------
// Helpers
//------------------------------------------------------------------------------

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
 *
 * @param {ASTNode} node - A function node to get.
 * @returns {ASTNode|Token} The node or the token of a location.
 */
function getId(node) {
    return node.id || node;
}

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = {
    meta: {
        docs: {
            description: "enforce `return` statements in getters",
            category: "Possible Errors",
            recommended: false
        },
        fixable: null,
        schema: [
            {
                type: "object",
                properties: {
                    allowImplicit: {
                        type: "boolean"
                    }
                },
                additionalProperties: false
            }
        ]
    },

    create(context) {

        const options = context.options[0] || { allowImplicit: false };

        let funcInfo = {
            upper: null,
            codePath: null,
            hasReturn: false,
            shouldCheck: false,
            node: null
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
                    node,
                    loc: getId(node).loc.start,
                    message: funcInfo.hasReturn
                        ? "Expected {{name}} to always return a value."
                        : "Expected to return a value in {{name}}.",
                    data: {
                        name: astUtils.getFunctionNameWithKind(funcInfo.node)
                    }
                });
            }
        }

        return {

            // Stacks this function's information.
            onCodePathStart(codePath, node) {
                const parent = node.parent;

                funcInfo = {
                    upper: funcInfo,
                    codePath,
                    hasReturn: false,
                    shouldCheck:
                        node.type === "FunctionExpression" &&
                        node.body.type === "BlockStatement" &&

                        // check if it is a "getter", or a method named "get".
                        (parent.kind === "get" || astUtils.getStaticPropertyName(parent) === "get"),
                    node
                };
            },

            // Pops this function's information.
            onCodePathEnd() {
                funcInfo = funcInfo.upper;
            },

            // Checks the return statement is valid.
            ReturnStatement(node) {
                if (funcInfo.shouldCheck) {
                    funcInfo.hasReturn = true;

                    // if allowImplicit: false, should also check node.argument
                    if (!options.allowImplicit && !node.argument) {
                        context.report({
                            node,
                            message: "Expected to return a value in {{name}}.",
                            data: {
                                name: astUtils.getFunctionNameWithKind(funcInfo.node)
                            }
                        });
                    }
                }
            },

            // Reports a given function if the last path is reachable.
            "FunctionExpression:exit": checkLastSegment
        };
    }
};
