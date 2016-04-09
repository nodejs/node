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

    /*
     * Information for each constructor.
     * - upper:      Information of the upper constructor.
     * - hasExtends: A flag which shows whether the owner class has a valid
     *   `extends` part.
     * - scope:      The scope of the owner class.
     * - codePath:   The code path of this constructor.
     */
    var funcInfo = null;

    /*
     * Information for each code path segment.
     * Each key is the id of a code path segment.
     * Each value is an object:
     * - superCalled:  The flag which shows `super()` called in all code paths.
     * - invalidNodes: The array of invalid ThisExpression and Super nodes.
     */
    var segInfoMap = Object.create(null);

    /**
     * Gets whether or not `super()` is called in a given code path segment.
     * @param {CodePathSegment} segment - A code path segment to get.
     * @returns {boolean} `true` if `super()` is called.
     */
    function isCalled(segment) {
        return segInfoMap[segment.id].superCalled;
    }

    /**
     * Checks whether or not this is in a constructor.
     * @returns {boolean} `true` if this is in a constructor.
     */
    function isInConstructorOfDerivedClass() {
        return Boolean(funcInfo && funcInfo.isConstructor && funcInfo.hasExtends);
    }

    /**
     * Checks whether or not this is before `super()` is called.
     * @returns {boolean} `true` if this is before `super()` is called.
     */
    function isBeforeCallOfSuper() {
        return (
            isInConstructorOfDerivedClass(funcInfo) &&
            !funcInfo.codePath.currentSegments.every(isCalled)
        );
    }

    /**
     * Sets a given node as invalid.
     * @param {ASTNode} node - A node to set as invalid. This is one of
     *      a ThisExpression and a Super.
     * @returns {void}
     */
    function setInvalid(node) {
        var segments = funcInfo.codePath.currentSegments;

        for (var i = 0; i < segments.length; ++i) {
            segInfoMap[segments[i].id].invalidNodes.push(node);
        }
    }

    /**
     * Sets the current segment as `super` was called.
     * @returns {void}
     */
    function setSuperCalled() {
        var segments = funcInfo.codePath.currentSegments;

        for (var i = 0; i < segments.length; ++i) {
            segInfoMap[segments[i].id].superCalled = true;
        }
    }

    return {

        /**
         * Adds information of a constructor into the stack.
         * @param {CodePath} codePath - A code path which was started.
         * @param {ASTNode} node - The current node.
         * @returns {void}
         */
        "onCodePathStart": function(codePath, node) {
            if (isConstructorFunction(node)) {

                // Class > ClassBody > MethodDefinition > FunctionExpression
                var classNode = node.parent.parent.parent;

                funcInfo = {
                    upper: funcInfo,
                    isConstructor: true,
                    hasExtends: Boolean(
                        classNode.superClass &&
                        !astUtils.isNullOrUndefined(classNode.superClass)
                    ),
                    codePath: codePath
                };
            } else {
                funcInfo = {
                    upper: funcInfo,
                    isConstructor: false,
                    hasExtends: false,
                    codePath: codePath
                };
            }
        },

        /**
         * Removes the top of stack item.
         *
         * And this treverses all segments of this code path then reports every
         * invalid node.
         *
         * @param {CodePath} codePath - A code path which was ended.
         * @param {ASTNode} node - The current node.
         * @returns {void}
         */
        "onCodePathEnd": function(codePath) {
            var isDerivedClass = funcInfo.hasExtends;

            funcInfo = funcInfo.upper;
            if (!isDerivedClass) {
                return;
            }

            codePath.traverseSegments(function(segment, controller) {
                var info = segInfoMap[segment.id];

                for (var i = 0; i < info.invalidNodes.length; ++i) {
                    var invalidNode = info.invalidNodes[i];

                    context.report({
                        message: "'{{kind}}' is not allowed before 'super()'.",
                        node: invalidNode,
                        data: {
                            kind: invalidNode.type === "Super" ? "super" : "this"
                        }
                    });
                }

                if (info.superCalled) {
                    controller.skip();
                }
            });
        },

        /**
         * Initialize information of a given code path segment.
         * @param {CodePathSegment} segment - A code path segment to initialize.
         * @returns {void}
         */
        "onCodePathSegmentStart": function(segment) {
            if (!isInConstructorOfDerivedClass(funcInfo)) {
                return;
            }

            // Initialize info.
            segInfoMap[segment.id] = {
                superCalled: (
                    segment.prevSegments.length > 0 &&
                    segment.prevSegments.every(isCalled)
                ),
                invalidNodes: []
            };
        },

        /**
         * Update information of the code path segment when a code path was
         * looped.
         * @param {CodePathSegment} fromSegment - The code path segment of the
         *      end of a loop.
         * @param {CodePathSegment} toSegment - A code path segment of the head
         *      of a loop.
         * @returns {void}
         */
        "onCodePathSegmentLoop": function(fromSegment, toSegment) {
            if (!isInConstructorOfDerivedClass(funcInfo)) {
                return;
            }

            // Update information inside of the loop.
            funcInfo.codePath.traverseSegments(
                {first: toSegment, last: fromSegment},
                function(segment, controller) {
                    var info = segInfoMap[segment.id];

                    if (info.superCalled) {
                        info.invalidNodes = [];
                        controller.skip();
                    } else if (
                        segment.prevSegments.length > 0 &&
                        segment.prevSegments.every(isCalled)
                    ) {
                        info.superCalled = true;
                        info.invalidNodes = [];
                    }
                }
            );
        },

        /**
         * Reports if this is before `super()`.
         * @param {ASTNode} node - A target node.
         * @returns {void}
         */
        "ThisExpression": function(node) {
            if (isBeforeCallOfSuper()) {
                setInvalid(node);
            }
        },

        /**
         * Reports if this is before `super()`.
         * @param {ASTNode} node - A target node.
         * @returns {void}
         */
        "Super": function(node) {
            if (!astUtils.isCallee(node) && isBeforeCallOfSuper()) {
                setInvalid(node);
            }
        },

        /**
         * Marks `super()` called.
         * @param {ASTNode} node - A target node.
         * @returns {void}
         */
        "CallExpression:exit": function(node) {
            if (node.callee.type === "Super" && isBeforeCallOfSuper()) {
                setSuperCalled();
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
