/**
 * @fileoverview A rule to verify `super()` callings in constructor.
 * @author Toru Nagashima
 * @copyright 2015 Toru Nagashima. All rights reserved.
 */

"use strict";

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = function(context) {

    /**
     * Searches a class node from ancestors of a node.
     * @param {Node} node - A node to get.
     * @returns {ClassDeclaration|ClassExpression|null} the found class node or `null`.
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
     * Checks whether or not the current traversal context is on constructors.
     * @param {{scope: Scope}} item - A checking context to check.
     * @returns {boolean} whether or not the current traversal context is on constructors.
     */
    function isOnConstructor(item) {
        return item && item.scope === context.getScope().variableScope.upper.variableScope;
    }

    // A stack for checking context.
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
                superCallings: [],
                scope: context.getScope().variableScope
            });
        },

        /**
         * Checks the result, then reports invalid/missing `super()`.
         * @param {MethodDefinition} node - A target node.
         * @returns {void}
         */
        "MethodDefinition:exit": function(node) {
            if (node.kind !== "constructor") {
                return;
            }
            var result = stack.pop();

            var classNode = getClassInAncestor(node);
            /* istanbul ignore if */
            if (!classNode) {
                return;
            }

            if (classNode.superClass === null || isNullLiteral(classNode.superClass)) {
                result.superCallings.forEach(function(superCalling) {
                    context.report(superCalling, "unexpected `super()`.");
                });
            } else if (result.superCallings.length === 0) {
                context.report(node.key, "this constructor requires `super()`.");
            }
        },

        /**
         * Checks the result of checking, then reports invalid/missing `super()`.
         * @param {MethodDefinition} node - A target node.
         * @returns {void}
         */
        "CallExpression": function(node) {
            var item = stack[stack.length - 1];
            if (isOnConstructor(item) && node.callee.type === "Super") {
                item.superCallings.push(node);
            }
        }
    };
};

module.exports.schema = [];
