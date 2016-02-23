/**
 * @fileoverview A rule to verify `super()` callings in constructor.
 * @author Toru Nagashima
 * @copyright 2015 Toru Nagashima. All rights reserved.
 */

"use strict";

//------------------------------------------------------------------------------
// Requirements
//------------------------------------------------------------------------------

var astUtils = require("../ast-utils");

//------------------------------------------------------------------------------
// Helpers
//------------------------------------------------------------------------------

/**
 * Checks whether or not a given node is a constructor.
 * @param {ASTNode} node - A node to check. This node type is one of
 *   `Program`, `FunctionDeclaration`, `FunctionExpression`, and
 *   `ArrowFunctionExpression`.
 * @returns {boolean} `true` if the node is a constructor.
 */
function isConstructorFunction(node) {
    return (
        node.type === "FunctionExpression" &&
        node.parent.type === "MethodDefinition" &&
        node.parent.kind === "constructor"
    );
}

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = function(context) {
    // {{hasExtends: boolean, scope: Scope, codePath: CodePath}[]}
    // Information for each constructor.
    // - upper:      Information of the upper constructor.
    // - hasExtends: A flag which shows whether own class has a valid `extends`
    //               part.
    // - scope:      The scope of own class.
    // - codePath:   The code path object of the constructor.
    var funcInfo = null;

    // {Map<string, {calledInSomePaths: boolean, calledInEveryPaths: boolean}>}
    // Information for each code path segment.
    // - calledInSomePaths:  A flag of be called `super()` in some code paths.
    // - calledInEveryPaths: A flag of be called `super()` in all code paths.
    var segInfoMap = Object.create(null);

    /**
     * Gets the flag which shows `super()` is called in some paths.
     * @param {CodePathSegment} segment - A code path segment to get.
     * @returns {boolean} The flag which shows `super()` is called in some paths
     */
    function isCalledInSomePath(segment) {
        return segInfoMap[segment.id].calledInSomePaths;
    }

    /**
     * Gets the flag which shows `super()` is called in all paths.
     * @param {CodePathSegment} segment - A code path segment to get.
     * @returns {boolean} The flag which shows `super()` is called in all paths.
     */
    function isCalledInEveryPath(segment) {
        return segInfoMap[segment.id].calledInEveryPaths;
    }

    return {
        /**
         * Stacks a constructor information.
         * @param {CodePath} codePath - A code path which was started.
         * @param {ASTNode} node - The current node.
         * @returns {void}
         */
        "onCodePathStart": function(codePath, node) {
            if (!isConstructorFunction(node)) {
                return;
            }

            // Class > ClassBody > MethodDefinition > FunctionExpression
            var classNode = node.parent.parent.parent;
            funcInfo = {
                upper: funcInfo,
                hasExtends: Boolean(
                    classNode.superClass &&
                    !astUtils.isNullOrUndefined(classNode.superClass)
                ),
                scope: context.getScope(),
                codePath: codePath
            };
        },

        /**
         * Pops a constructor information.
         * And reports if `super()` lacked.
         * @param {CodePath} codePath - A code path which was ended.
         * @param {ASTNode} node - The current node.
         * @returns {void}
         */
        "onCodePathEnd": function(codePath, node) {
            if (!isConstructorFunction(node)) {
                return;
            }

            // Skip if own class which has a valid `extends` part.
            var hasExtends = funcInfo.hasExtends;
            funcInfo = funcInfo.upper;
            if (!hasExtends) {
                return;
            }

            // Reports if `super()` lacked.
            var segments = codePath.returnedSegments;
            var calledInEveryPaths = segments.every(isCalledInEveryPath);
            var calledInSomePaths = segments.some(isCalledInSomePath);
            if (!calledInEveryPaths) {
                context.report({
                    message: calledInSomePaths ?
                        "Lacked a call of 'super()' in some code paths." :
                        "Expected to call 'super()'.",
                    node: node.parent
                });
            }
        },

        /**
         * Initialize information of a given code path segment.
         * @param {CodePathSegment} segment - A code path segment to initialize.
         * @returns {void}
         */
        "onCodePathSegmentStart": function(segment) {
            // Skip if this is not in a constructor of a class which has a valid
            // `extends` part.
            if (!(
                funcInfo &&
                funcInfo.hasExtends &&
                funcInfo.scope === context.getScope().variableScope
            )) {
                return;
            }

            // Initialize info.
            var info = segInfoMap[segment.id] = {
                calledInSomePaths: false,
                calledInEveryPaths: false
            };

            // When there are previous segments, aggregates these.
            var prevSegments = segment.prevSegments;
            if (prevSegments.length > 0) {
                info.calledInSomePaths = prevSegments.some(isCalledInSomePath);
                info.calledInEveryPaths = prevSegments.every(isCalledInEveryPath);
            }
        },

        /**
         * Checks for a call of `super()`.
         * @param {ASTNode} node - A CallExpression node to check.
         * @returns {void}
         */
        "CallExpression:exit": function(node) {
            // Skip if the node is not `super()`.
            if (node.callee.type !== "Super") {
                return;
            }

            // Skip if this is not in a constructor.
            if (!(funcInfo && funcInfo.scope === context.getScope().variableScope)) {
                return;
            }

            // Reports if needed.
            if (funcInfo.hasExtends) {
                // This class has a valid `extends` part.
                // Checks duplicate `super()`;
                var segments = funcInfo.codePath.currentSegments;
                var duplicate = false;
                for (var i = 0; i < segments.length; ++i) {
                    var info = segInfoMap[segments[i].id];

                    duplicate = duplicate || info.calledInSomePaths;
                    info.calledInSomePaths = info.calledInEveryPaths = true;
                }

                if (duplicate) {
                    context.report({
                        message: "Unexpected duplicate 'super()'.",
                        node: node
                    });
                }
            } else {
                // This class does not have a valid `extends` part.
                // Disallow `super()`.
                context.report({
                    message: "Unexpected 'super()'.",
                    node: node
                });
            }
        },

        /**
         * Resets state.
         * @returns {void}
         */
        "Program:exit": function() {
            segInfoMap = Object.create(null);
        }
    };
};

module.exports.schema = [];
