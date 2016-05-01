/**
 * @fileoverview Rule to flag unnecessary bind calls
 * @author Bence Dányi <bence@danyi.me>
 */
"use strict";

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = {
    meta: {
        docs: {
            description: "disallow unnecessary calls to `.bind()`",
            category: "Best Practices",
            recommended: false
        },

        schema: []
    },

    create: function(context) {
        var scopeInfo = null;

        /**
         * Reports a given function node.
         *
         * @param {ASTNode} node - A node to report. This is a FunctionExpression or
         *      an ArrowFunctionExpression.
         * @returns {void}
         */
        function report(node) {
            context.report({
                node: node.parent.parent,
                message: "The function binding is unnecessary.",
                loc: node.parent.property.loc.start
            });
        }

        /**
         * Gets the property name of a given node.
         * If the property name is dynamic, this returns an empty string.
         *
         * @param {ASTNode} node - A node to check. This is a MemberExpression.
         * @returns {string} The property name of the node.
         */
        function getPropertyName(node) {
            if (node.computed) {
                switch (node.property.type) {
                    case "Literal":
                        return String(node.property.value);
                    case "TemplateLiteral":
                        if (node.property.expressions.length === 0) {
                            return node.property.quasis[0].value.cooked;
                        }

                        // fallthrough
                    default:
                        return false;
                }
            }
            return node.property.name;
        }

        /**
         * Checks whether or not a given function node is the callee of `.bind()`
         * method.
         *
         * e.g. `(function() {}.bind(foo))`
         *
         * @param {ASTNode} node - A node to report. This is a FunctionExpression or
         *      an ArrowFunctionExpression.
         * @returns {boolean} `true` if the node is the callee of `.bind()` method.
         */
        function isCalleeOfBindMethod(node) {
            var parent = node.parent;
            var grandparent = parent.parent;

            return (
                grandparent &&
                grandparent.type === "CallExpression" &&
                grandparent.callee === parent &&
                grandparent.arguments.length === 1 &&
                parent.type === "MemberExpression" &&
                parent.object === node &&
                getPropertyName(parent) === "bind"
            );
        }

        /**
         * Adds a scope information object to the stack.
         *
         * @param {ASTNode} node - A node to add. This node is a FunctionExpression
         *      or a FunctionDeclaration node.
         * @returns {void}
         */
        function enterFunction(node) {
            scopeInfo = {
                isBound: isCalleeOfBindMethod(node),
                thisFound: false,
                upper: scopeInfo
            };
        }

        /**
         * Removes the scope information object from the top of the stack.
         * At the same time, this reports the function node if the function has
         * `.bind()` and the `this` keywords found.
         *
         * @param {ASTNode} node - A node to remove. This node is a
         *      FunctionExpression or a FunctionDeclaration node.
         * @returns {void}
         */
        function exitFunction(node) {
            if (scopeInfo.isBound && !scopeInfo.thisFound) {
                report(node);
            }

            scopeInfo = scopeInfo.upper;
        }

        /**
         * Reports a given arrow function if the function is callee of `.bind()`
         * method.
         *
         * @param {ASTNode} node - A node to report. This node is an
         *      ArrowFunctionExpression.
         * @returns {void}
         */
        function exitArrowFunction(node) {
            if (isCalleeOfBindMethod(node)) {
                report(node);
            }
        }

        /**
         * Set the mark as the `this` keyword was found in this scope.
         *
         * @returns {void}
         */
        function markAsThisFound() {
            if (scopeInfo) {
                scopeInfo.thisFound = true;
            }
        }

        return {
            "ArrowFunctionExpression:exit": exitArrowFunction,
            FunctionDeclaration: enterFunction,
            "FunctionDeclaration:exit": exitFunction,
            FunctionExpression: enterFunction,
            "FunctionExpression:exit": exitFunction,
            ThisExpression: markAsThisFound
        };
    }
};
