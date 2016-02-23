/**
 * @fileoverview A rule to disallow using `this`/`super` before `super()`.
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
    // {{hasExtends: boolean, scope: Scope}[]}
    // Information for each constructor.
    // - upper:      Information of the upper constructor.
    // - hasExtends: A flag which shows whether the owner class has a valid
    //               `extends` part.
    // - scope:      The scope of the owner class.
    // - codePath:   The code path of this constructor.
    var funcInfo = null;

    // {Map<string, boolean>}
    // Information for each code path segment.
    // The value is a flag which shows `super()` called in all code paths.
    var segInfoMap = Object.create(null);

    /**
     * Gets whether or not `super()` is called in a given code path segment.
     * @param {CodePathSegment} segment - A code path segment to get.
     * @returns {boolean} `true` if `super()` is called.
     */
    function isCalled(segment) {
        return Boolean(segInfoMap[segment.id]);
    }

    /**
     * Checks whether or not this is in a constructor.
     * @returns {boolean} `true` if this is in a constructor.
     */
    function isInConstructor() {
        return Boolean(
            funcInfo &&
            funcInfo.hasExtends &&
            funcInfo.scope === context.getScope().variableScope
        );
    }

    /**
     * Checks whether or not this is before `super()` is called.
     * @returns {boolean} `true` if this is before `super()` is called.
     */
    function isBeforeCallOfSuper() {
        return (
            isInConstructor(funcInfo) &&
            !funcInfo.codePath.currentSegments.every(isCalled)
        );
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
         * @param {CodePath} codePath - A code path which was ended.
         * @param {ASTNode} node - The current node.
         * @returns {void}
         */
        "onCodePathEnd": function(codePath, node) {
            if (isConstructorFunction(node)) {
                funcInfo = funcInfo.upper;
            }
        },

        /**
         * Initialize information of a given code path segment.
         * @param {CodePathSegment} segment - A code path segment to initialize.
         * @returns {void}
         */
        "onCodePathSegmentStart": function(segment) {
            if (!isInConstructor(funcInfo)) {
                return;
            }

            // Initialize info.
            segInfoMap[segment.id] = (
                segment.prevSegments.length > 0 &&
                segment.prevSegments.every(isCalled)
            );
        },

        /**
         * Reports if this is before `super()`.
         * @param {ASTNode} node - A target node.
         * @returns {void}
         */
        "ThisExpression": function(node) {
            if (isBeforeCallOfSuper()) {
                context.report({
                    message: "'this' is not allowed before 'super()'.",
                    node: node
                });
            }
        },

        /**
         * Reports if this is before `super()`.
         * @param {ASTNode} node - A target node.
         * @returns {void}
         */
        "Super": function(node) {
            if (!astUtils.isCallee(node) && isBeforeCallOfSuper()) {
                context.report({
                    message: "'super' is not allowed before 'super()'.",
                    node: node
                });
            }
        },

        /**
         * Marks `super()` called.
         * @param {ASTNode} node - A target node.
         * @returns {void}
         */
        "CallExpression:exit": function(node) {
            if (node.callee.type === "Super" && isBeforeCallOfSuper()) {
                var segments = funcInfo.codePath.currentSegments;
                for (var i = 0; i < segments.length; ++i) {
                    segInfoMap[segments[i].id] = true;
                }
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
