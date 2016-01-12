/**
 * @fileoverview A rule to disallow `this` keywords outside of classes or class-like objects.
 * @author Toru Nagashima
 * @copyright 2015 Toru Nagashima. All rights reserved.
 * See LICENSE file in root directory for full license.
 */

"use strict";

//------------------------------------------------------------------------------
// Requirements
//------------------------------------------------------------------------------

var astUtils = require("../ast-utils");

//------------------------------------------------------------------------------
// Helpers
//------------------------------------------------------------------------------

var thisTagPattern = /^[\s\*]*@this/m;
var anyFunctionPattern = /^(?:Function(?:Declaration|Expression)|ArrowFunctionExpression)$/;
var bindOrCallOrApplyPattern = /^(?:bind|call|apply)$/;
var arrayOrTypedArrayPattern = /Array$/;
var arrayMethodPattern = /^(?:every|filter|find|findIndex|forEach|map|some)$/;

/**
 * Checks whether or not a node is a constructor.
 * @param {ASTNode} node - A function node to check.
 * @returns {boolean} Wehether or not a node is a constructor.
 */
function isES5Constructor(node) {
    return (
        node.id &&
        node.id.name[0] === node.id.name[0].toLocaleUpperCase()
    );
}

/**
 * Finds a function node from ancestors of a node.
 * @param {ASTNode} node - A start node to find.
 * @returns {Node|null} A found function node.
 */
function getUpperFunction(node) {
    while (node) {
        if (anyFunctionPattern.test(node.type)) {
            return node;
        }
        node = node.parent;
    }
    return null;
}

/**
 * Checks whether or not a node is callee.
 * @param {ASTNode} node - A node to check.
 * @returns {boolean} Whether or not the node is callee.
 */
function isCallee(node) {
    return node.parent.type === "CallExpression" && node.parent.callee === node;
}

/**
 * Checks whether or not a node is `Reclect.apply`.
 * @param {ASTNode} node - A node to check.
 * @returns {boolean} Whether or not the node is a `Reclect.apply`.
 */
function isReflectApply(node) {
    return (
        node.type === "MemberExpression" &&
        node.object.type === "Identifier" &&
        node.object.name === "Reflect" &&
        node.property.type === "Identifier" &&
        node.property.name === "apply" &&
        node.computed === false
    );
}

/**
 * Checks whether or not a node is `Array.from`.
 * @param {ASTNode} node - A node to check.
 * @returns {boolean} Whether or not the node is a `Array.from`.
 */
function isArrayFrom(node) {
    return (
        node.type === "MemberExpression" &&
        node.object.type === "Identifier" &&
        arrayOrTypedArrayPattern.test(node.object.name) &&
        node.property.type === "Identifier" &&
        node.property.name === "from" &&
        node.computed === false
    );
}

/**
 * Checks whether or not a node is a method which has `thisArg`.
 * @param {ASTNode} node - A node to check.
 * @returns {boolean} Whether or not the node is a method which has `thisArg`.
 */
function isMethodWhichHasThisArg(node) {
    while (node) {
        if (node.type === "Identifier") {
            return arrayMethodPattern.test(node.name);
        }
        if (node.type === "MemberExpression" && !node.computed) {
            node = node.property;
            continue;
        }

        break;
    }

    return false;
}

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = function(context) {
    var stack = [],
        sourceCode = context.getSourceCode();


    /**
     * Checks whether or not a node has a `@this` tag in its comments.
     * @param {ASTNode} node - A node to check.
     * @returns {boolean} Whether or not the node has a `@this` tag in its comments.
     */
    function hasJSDocThisTag(node) {
        var jsdocComment = sourceCode.getJSDocComment(node);
        if (jsdocComment && thisTagPattern.test(jsdocComment.value)) {
            return true;
        }

        // Checks `@this` in its leading comments for callbacks,
        // because callbacks don't have its JSDoc comment.
        // e.g.
        //     sinon.test(/* @this sinon.Sandbox */function() { this.spy(); });
        return sourceCode.getComments(node).leading.some(function(comment) {
            return thisTagPattern.test(comment.value);
        });
    }

    /**
     * Checks whether or not a node has valid `this`.
     *
     * First, this checks the node:
     *
     * - The function name starts with uppercase (it's a constructor).
     * - The function has a JSDoc comment that has a @this tag.
     *
     * Next, this checks the location of the node.
     * If the location is below, this judges `this` is valid.
     *
     * - The location is on an object literal.
     * - The location assigns to a property.
     * - The location is on an ES2015 class.
     * - The location calls its `bind`/`call`/`apply` method directly.
     * - The function is a callback of array methods (such as `.forEach()`) if `thisArg` is given.
     *
     * @param {ASTNode} node - A node to check.
     * @returns {boolean} A found function node.
     */
    function hasValidThis(node) {
        if (isES5Constructor(node) || hasJSDocThisTag(node)) {
            return true;
        }

        while (node) {
            var parent = node.parent;
            switch (parent.type) {
                // Looks up the destination.
                // e.g.
                //   obj.foo = nativeFoo || function foo() { ... };
                case "LogicalExpression":
                case "ConditionalExpression":
                    node = parent;
                    break;

                // If the upper function is IIFE, checks the destination of the return value.
                // e.g.
                //   obj.foo = (function() {
                //     // setup...
                //     return function foo() { ... };
                //   })();
                case "ReturnStatement":
                    var func = getUpperFunction(parent);
                    if (func === null || !isCallee(func)) {
                        return false;
                    }
                    node = func.parent;
                    break;

                // e.g.
                //   var obj = { foo() { ... } };
                //   var obj = { foo: function() { ... } };
                case "Property":
                    return true;

                // e.g.
                //   obj.foo = foo() { ... };
                case "AssignmentExpression":
                    return (
                        parent.right === node &&
                        parent.left.type === "MemberExpression"
                    );

                // e.g.
                //   class A { constructor() { ... } }
                //   class A { foo() { ... } }
                //   class A { get foo() { ... } }
                //   class A { set foo() { ... } }
                //   class A { static foo() { ... } }
                case "MethodDefinition":
                    return !parent.static;

                // e.g.
                //   var foo = function foo() { ... }.bind(obj);
                //   (function foo() { ... }).call(obj);
                //   (function foo() { ... }).apply(obj, []);
                case "MemberExpression":
                    return (
                        parent.object === node &&
                        parent.property.type === "Identifier" &&
                        bindOrCallOrApplyPattern.test(parent.property.name) &&
                        isCallee(parent) &&
                        parent.parent.arguments.length > 0 &&
                        !astUtils.isNullOrUndefined(parent.parent.arguments[0])
                    );

                // e.g.
                //   Reflect.apply(function() {}, obj, []);
                //   Array.from([], function() {}, obj);
                //   list.forEach(function() {}, obj);
                case "CallExpression":
                    if (isReflectApply(parent.callee)) {
                        return (
                            parent.arguments.length === 3 &&
                            parent.arguments[0] === node &&
                            !astUtils.isNullOrUndefined(parent.arguments[1])
                        );
                    }
                    if (isArrayFrom(parent.callee)) {
                        return (
                            parent.arguments.length === 3 &&
                            parent.arguments[1] === node &&
                            !astUtils.isNullOrUndefined(parent.arguments[2])
                        );
                    }
                    if (isMethodWhichHasThisArg(parent.callee)) {
                        return (
                            parent.arguments.length === 2 &&
                            parent.arguments[0] === node &&
                            !astUtils.isNullOrUndefined(parent.arguments[1])
                        );
                    }
                    return false;

                // Otherwise `this` is invalid.
                default:
                    return false;
            }
        }

        /* istanbul ignore next */
        throw new Error("unreachable");
    }

    /**
     * Gets the current checking context.
     *
     * The return value has a flag that whether or not `this` keyword is valid.
     * The flag is initialized when got at the first time.
     *
     * @returns {{valid: boolean}}
     *   an object which has a flag that whether or not `this` keyword is valid.
     */
    stack.getCurrent = function() {
        var current = this[this.length - 1];
        if (!current.init) {
            current.init = true;
            current.valid = hasValidThis(current.node);
        }
        return current;
    };

    /**
     * Pushs new checking context into the stack.
     *
     * The checking context is not initialized yet.
     * Because most functions don't have `this` keyword.
     * When `this` keyword was found, the checking context is initialized.
     *
     * @param {ASTNode} node - A function node that was entered.
     * @returns {void}
     */
    function enterFunction(node) {
        // `this` can be invalid only under strict mode.
        stack.push({
            init: !context.getScope().isStrict,
            node: node,
            valid: true
        });
    }

    /**
     * Pops the current checking context from the stack.
     * @returns {void}
     */
    function exitFunction() {
        stack.pop();
    }

    return {
        // `this` is invalid only under strict mode.
        // Modules is always strict mode.
        "Program": function(node) {
            var scope = context.getScope();
            var features = context.ecmaFeatures;

            stack.push({
                init: true,
                node: node,
                valid: !(
                    scope.isStrict ||
                    features.modules ||
                    (features.globalReturn && scope.childScopes[0].isStrict)
                )
            });
        },
        "Program:exit": function() {
            stack.pop();
        },

        "FunctionDeclaration": enterFunction,
        "FunctionDeclaration:exit": exitFunction,
        "FunctionExpression": enterFunction,
        "FunctionExpression:exit": exitFunction,

        // Reports if `this` of the current context is invalid.
        "ThisExpression": function(node) {
            var current = stack.getCurrent();
            if (current && !current.valid) {
                context.report(node, "Unexpected `this`.");
            }
        }
    };
};

module.exports.schema = [];
