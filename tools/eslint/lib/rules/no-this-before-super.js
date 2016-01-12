/**
 * @fileoverview A rule to disallow using `this`/`super` before `super()`.
 * @author Toru Nagashima
 * @copyright 2015 Toru Nagashima. All rights reserved.
 */

"use strict";

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = function(context) {

    /**
     * Searches a class node that a node is belonging to.
     * @param {Node} node - A node to start searching.
     * @returns {ClassDeclaration|ClassExpression|null} the found class node, or `null`.
     */
    function getClassInAncestor(node) {
        while (node) {
            if (node.type === "ClassDeclaration" || node.type === "ClassExpression") {
                return node;
            }
            node = node.parent;
        }
        /* istanbul ignore next */
        return null;
    }

    /**
     * Checks whether or not a node is the null literal.
     * @param {Node} node - A node to check.
     * @returns {boolean} whether or not a node is the null literal.
     */
    function isNullLiteral(node) {
        return node && node.type === "Literal" && node.value === null;
    }

    /**
     * Checks whether or not a node is the callee of a call expression.
     * @param {Node} node - A node to check.
     * @returns {boolean} whether or not a node is the callee of a call expression.
     */
    function isCallee(node) {
        return node && node.parent.type === "CallExpression" && node.parent.callee === node;
    }

    /**
     * Checks whether or not the current traversal context is before `super()`.
     * @param {object} item - A checking context.
     * @returns {boolean} whether or not the current traversal context is before `super()`.
     */
    function isBeforeSuperCalling(item) {
        return (
            item &&
            item.scope === context.getScope().variableScope.upper.variableScope &&
            item.superCalled === false
        );
    }

    var stack = [];

    return {
        /**
         * Start checking.
         * @param {MethodDefinition} node - A target node.
         * @returns {void}
         */
        "MethodDefinition": function(node) {
            if (node.kind !== "constructor") {
                return;
            }
            stack.push({
                thisOrSuperBeforeSuperCalled: [],
                superCalled: false,
                scope: context.getScope().variableScope
            });
        },

        /**
         * Treats the result of checking and reports invalid `this`/`super`.
         * @param {MethodDefinition} node - A target node.
         * @returns {void}
         */
        "MethodDefinition:exit": function(node) {
            if (node.kind !== "constructor") {
                return;
            }
            var result = stack.pop();

            // Skip if it has no extends or `extends null`.
            var classNode = getClassInAncestor(node);
            if (!classNode || !classNode.superClass || isNullLiteral(classNode.superClass)) {
                return;
            }

            // Reports.
            result.thisOrSuperBeforeSuperCalled.forEach(function(thisOrSuper) {
                var type = (thisOrSuper.type === "Super" ? "super" : "this");
                context.report(thisOrSuper, "\"{{type}}\" is not allowed before super()", {type: type});
            });
        },

        /**
         * Marks the node if is before `super()`.
         * @param {ThisExpression} node - A target node.
         * @returns {void}
         */
        "ThisExpression": function(node) {
            var item = stack[stack.length - 1];
            if (isBeforeSuperCalling(item)) {
                item.thisOrSuperBeforeSuperCalled.push(node);
            }
        },

        /**
         * Marks the node if is before `super()`. (exclude `super()` itself)
         * @param {Super} node - A target node.
         * @returns {void}
         */
        "Super": function(node) {
            var item = stack[stack.length - 1];
            if (isBeforeSuperCalling(item) && isCallee(node) === false) {
                item.thisOrSuperBeforeSuperCalled.push(node);
            }
        },

        /**
         * Marks `super()` called.
         * To catch `super(this.a);`, marks on `CallExpression:exit`.
         * @param {CallExpression} node - A target node.
         * @returns {void}
         */
        "CallExpression:exit": function(node) {
            var item = stack[stack.length - 1];
            if (isBeforeSuperCalling(item) && node.callee.type === "Super") {
                item.superCalled = true;
            }
        }
    };
};

module.exports.schema = [];
