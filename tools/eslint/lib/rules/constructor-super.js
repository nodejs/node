/**
 * @fileoverview A rule to verify `super()` callings in constructor.
 * @author Toru Nagashima
 */

"use strict";

//------------------------------------------------------------------------------
// Helpers
//------------------------------------------------------------------------------

/**
 * Checks whether a given code path segment is reachable or not.
 *
 * @param {CodePathSegment} segment - A code path segment to check.
 * @returns {boolean} `true` if the segment is reachable.
 */
function isReachable(segment) {
    return segment.reachable;
}

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

/**
 * Checks whether a given node can be a constructor or not.
 *
 * @param {ASTNode} node - A node to check.
 * @returns {boolean} `true` if the node can be a constructor.
 */
function isPossibleConstructor(node) {
    if (!node) {
        return false;
    }

    switch (node.type) {
        case "ClassExpression":
        case "FunctionExpression":
        case "ThisExpression":
        case "MemberExpression":
        case "CallExpression":
        case "NewExpression":
        case "YieldExpression":
        case "TaggedTemplateExpression":
        case "MetaProperty":
            return true;

        case "Identifier":
            return node.name !== "undefined";

        case "AssignmentExpression":
            return isPossibleConstructor(node.right);

        case "LogicalExpression":
            return (
                isPossibleConstructor(node.left) ||
                isPossibleConstructor(node.right)
            );

        case "ConditionalExpression":
            return (
                isPossibleConstructor(node.alternate) ||
                isPossibleConstructor(node.consequent)
            );

        case "SequenceExpression":
            var lastExpression = node.expressions[node.expressions.length - 1];

            return isPossibleConstructor(lastExpression);

        default:
            return false;
    }
}

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = {
    meta: {
        docs: {
            description: "require `super()` calls in constructors",
            category: "ECMAScript 6",
            recommended: true
        },

        schema: []
    },

    create: function(context) {

        /*
         * {{hasExtends: boolean, scope: Scope, codePath: CodePath}[]}
         * Information for each constructor.
         * - upper:      Information of the upper constructor.
         * - hasExtends: A flag which shows whether own class has a valid `extends`
         *               part.
         * - scope:      The scope of own class.
         * - codePath:   The code path object of the constructor.
         */
        var funcInfo = null;

        /*
         * {Map<string, {calledInSomePaths: boolean, calledInEveryPaths: boolean}>}
         * Information for each code path segment.
         * - calledInSomePaths:  A flag of be called `super()` in some code paths.
         * - calledInEveryPaths: A flag of be called `super()` in all code paths.
         * - validNodes:
         */
        var segInfoMap = Object.create(null);

        /**
         * Gets the flag which shows `super()` is called in some paths.
         * @param {CodePathSegment} segment - A code path segment to get.
         * @returns {boolean} The flag which shows `super()` is called in some paths
         */
        function isCalledInSomePath(segment) {
            return segment.reachable && segInfoMap[segment.id].calledInSomePaths;
        }

        /**
         * Gets the flag which shows `super()` is called in all paths.
         * @param {CodePathSegment} segment - A code path segment to get.
         * @returns {boolean} The flag which shows `super()` is called in all paths.
         */
        function isCalledInEveryPath(segment) {

            /*
             * If specific segment is the looped segment of the current segment,
             * skip the segment.
             * If not skipped, this never becomes true after a loop.
             */
            if (segment.nextSegments.length === 1 &&
                segment.nextSegments[0].isLoopedPrevSegment(segment)
            ) {
                return true;
            }
            return segment.reachable && segInfoMap[segment.id].calledInEveryPaths;
        }

        return {

            /**
             * Stacks a constructor information.
             * @param {CodePath} codePath - A code path which was started.
             * @param {ASTNode} node - The current node.
             * @returns {void}
             */
            onCodePathStart: function(codePath, node) {
                if (isConstructorFunction(node)) {

                    // Class > ClassBody > MethodDefinition > FunctionExpression
                    var classNode = node.parent.parent.parent;
                    var superClass = classNode.superClass;

                    funcInfo = {
                        upper: funcInfo,
                        isConstructor: true,
                        hasExtends: Boolean(superClass),
                        superIsConstructor: isPossibleConstructor(superClass),
                        codePath: codePath
                    };
                } else {
                    funcInfo = {
                        upper: funcInfo,
                        isConstructor: false,
                        hasExtends: false,
                        superIsConstructor: false,
                        codePath: codePath
                    };
                }
            },

            /**
             * Pops a constructor information.
             * And reports if `super()` lacked.
             * @param {CodePath} codePath - A code path which was ended.
             * @param {ASTNode} node - The current node.
             * @returns {void}
             */
            onCodePathEnd: function(codePath, node) {
                var hasExtends = funcInfo.hasExtends;

                // Pop.
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
            onCodePathSegmentStart: function(segment) {
                if (!(funcInfo && funcInfo.isConstructor && funcInfo.hasExtends)) {
                    return;
                }

                // Initialize info.
                var info = segInfoMap[segment.id] = {
                    calledInSomePaths: false,
                    calledInEveryPaths: false,
                    validNodes: []
                };

                // When there are previous segments, aggregates these.
                var prevSegments = segment.prevSegments;

                if (prevSegments.length > 0) {
                    info.calledInSomePaths = prevSegments.some(isCalledInSomePath);
                    info.calledInEveryPaths = prevSegments.every(isCalledInEveryPath);
                }
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
            onCodePathSegmentLoop: function(fromSegment, toSegment) {
                if (!(funcInfo && funcInfo.isConstructor && funcInfo.hasExtends)) {
                    return;
                }

                // Update information inside of the loop.
                var isRealLoop = toSegment.prevSegments.length >= 2;

                funcInfo.codePath.traverseSegments(
                    {first: toSegment, last: fromSegment},
                    function(segment) {
                        var info = segInfoMap[segment.id];
                        var prevSegments = segment.prevSegments;

                        // Updates flags.
                        info.calledInSomePaths = prevSegments.some(isCalledInSomePath);
                        info.calledInEveryPaths = prevSegments.every(isCalledInEveryPath);

                        // If flags become true anew, reports the valid nodes.
                        if (info.calledInSomePaths || isRealLoop) {
                            var nodes = info.validNodes;

                            info.validNodes = [];

                            for (var i = 0; i < nodes.length; ++i) {
                                var node = nodes[i];

                                context.report({
                                    message: "Unexpected duplicate 'super()'.",
                                    node: node
                                });
                            }
                        }
                    }
                );
            },

            /**
             * Checks for a call of `super()`.
             * @param {ASTNode} node - A CallExpression node to check.
             * @returns {void}
             */
            "CallExpression:exit": function(node) {
                if (!(funcInfo && funcInfo.isConstructor)) {
                    return;
                }

                // Skips except `super()`.
                if (node.callee.type !== "Super") {
                    return;
                }

                // Reports if needed.
                if (funcInfo.hasExtends) {
                    var segments = funcInfo.codePath.currentSegments;
                    var reachable = false;
                    var duplicate = false;

                    for (var i = 0; i < segments.length; ++i) {
                        var segment = segments[i];

                        if (segment.reachable) {
                            var info = segInfoMap[segment.id];

                            reachable = true;
                            duplicate = duplicate || info.calledInSomePaths;
                            info.calledInSomePaths = info.calledInEveryPaths = true;
                        }
                    }

                    if (reachable) {
                        if (duplicate) {
                            context.report({
                                message: "Unexpected duplicate 'super()'.",
                                node: node
                            });
                        } else if (!funcInfo.superIsConstructor) {
                            context.report({
                                message: "Unexpected 'super()' because 'super' is not a constructor.",
                                node: node
                            });
                        } else {
                            info.validNodes.push(node);
                        }
                    }
                } else if (funcInfo.codePath.currentSegments.some(isReachable)) {
                    context.report({
                        message: "Unexpected 'super()'.",
                        node: node
                    });
                }
            },

            /**
             * Set the mark to the returned path as `super()` was called.
             * @param {ASTNode} node - A ReturnStatement node to check.
             * @returns {void}
             */
            ReturnStatement: function(node) {
                if (!(funcInfo && funcInfo.isConstructor && funcInfo.hasExtends)) {
                    return;
                }

                // Skips if no argument.
                if (!node.argument) {
                    return;
                }

                // Returning argument is a substitute of 'super()'.
                var segments = funcInfo.codePath.currentSegments;

                for (var i = 0; i < segments.length; ++i) {
                    var segment = segments[i];

                    if (segment.reachable) {
                        var info = segInfoMap[segment.id];

                        info.calledInSomePaths = info.calledInEveryPaths = true;
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
    }
};
