/**
 * @fileoverview Prefer destructuring from arrays and objects
 * @author Alex LaFroscia
 */
"use strict";

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = {
    meta: {
        docs: {
            description: "require destructuring from arrays and/or objects",
            category: "ECMAScript 6",
            recommended: false,
            url: "https://eslint.org/docs/rules/prefer-destructuring"
        },
        schema: [
            {

                /*
                 * old support {array: Boolean, object: Boolean}
                 * new support {VariableDeclarator: {}, AssignmentExpression: {}}
                 */
                oneOf: [
                    {
                        type: "object",
                        properties: {
                            VariableDeclarator: {
                                type: "object",
                                properties: {
                                    array: {
                                        type: "boolean"
                                    },
                                    object: {
                                        type: "boolean"
                                    }
                                },
                                additionalProperties: false
                            },
                            AssignmentExpression: {
                                type: "object",
                                properties: {
                                    array: {
                                        type: "boolean"
                                    },
                                    object: {
                                        type: "boolean"
                                    }
                                },
                                additionalProperties: false
                            }
                        },
                        additionalProperties: false
                    },
                    {
                        type: "object",
                        properties: {
                            array: {
                                type: "boolean"
                            },
                            object: {
                                type: "boolean"
                            }
                        },
                        additionalProperties: false
                    }
                ]
            },
            {
                type: "object",
                properties: {
                    enforceForRenamedProperties: {
                        type: "boolean"
                    }
                },
                additionalProperties: false
            }
        ]
    },
    create(context) {

        const enabledTypes = context.options[0];
        const enforceForRenamedProperties = context.options[1] && context.options[1].enforceForRenamedProperties;
        let normalizedOptions = {
            VariableDeclarator: { array: true, object: true },
            AssignmentExpression: { array: true, object: true }
        };

        if (enabledTypes) {
            normalizedOptions = typeof enabledTypes.array !== "undefined" || typeof enabledTypes.object !== "undefined"
                ? { VariableDeclarator: enabledTypes, AssignmentExpression: enabledTypes }
                : enabledTypes;
        }

        //--------------------------------------------------------------------------
        // Helpers
        //--------------------------------------------------------------------------

        /**
         * @param {string} nodeType "AssignmentExpression" or "VariableDeclarator"
         * @param {string} destructuringType "array" or "object"
         * @returns {boolean} `true` if the destructuring type should be checked for the given node
         */
        function shouldCheck(nodeType, destructuringType) {
            return normalizedOptions &&
                normalizedOptions[nodeType] &&
                normalizedOptions[nodeType][destructuringType];
        }

        /**
         * Determines if the given node is accessing an array index
         *
         * This is used to differentiate array index access from object property
         * access.
         *
         * @param {ASTNode} node the node to evaluate
         * @returns {boolean} whether or not the node is an integer
         */
        function isArrayIndexAccess(node) {
            return Number.isInteger(node.property.value);
        }

        /**
         * Report that the given node should use destructuring
         *
         * @param {ASTNode} reportNode the node to report
         * @param {string} type the type of destructuring that should have been done
         * @returns {void}
         */
        function report(reportNode, type) {
            context.report({ node: reportNode, message: "Use {{type}} destructuring.", data: { type } });
        }

        /**
         * Check that the `prefer-destructuring` rules are followed based on the
         * given left- and right-hand side of the assignment.
         *
         * Pulled out into a separate method so that VariableDeclarators and
         * AssignmentExpressions can share the same verification logic.
         *
         * @param {ASTNode} leftNode the left-hand side of the assignment
         * @param {ASTNode} rightNode the right-hand side of the assignment
         * @param {ASTNode} reportNode the node to report the error on
         * @returns {void}
         */
        function performCheck(leftNode, rightNode, reportNode) {
            if (rightNode.type !== "MemberExpression" || rightNode.object.type === "Super") {
                return;
            }

            if (isArrayIndexAccess(rightNode)) {
                if (shouldCheck(reportNode.type, "array")) {
                    report(reportNode, "array");
                }
                return;
            }

            if (shouldCheck(reportNode.type, "object") && enforceForRenamedProperties) {
                report(reportNode, "object");
                return;
            }

            if (shouldCheck(reportNode.type, "object")) {
                const property = rightNode.property;

                if (
                    (property.type === "Literal" && leftNode.name === property.value) ||
                    (property.type === "Identifier" && leftNode.name === property.name && !rightNode.computed)
                ) {
                    report(reportNode, "object");
                }
            }
        }

        /**
         * Check if a given variable declarator is coming from an property access
         * that should be using destructuring instead
         *
         * @param {ASTNode} node the variable declarator to check
         * @returns {void}
         */
        function checkVariableDeclarator(node) {

            // Skip if variable is declared without assignment
            if (!node.init) {
                return;
            }

            // We only care about member expressions past this point
            if (node.init.type !== "MemberExpression") {
                return;
            }

            performCheck(node.id, node.init, node);
        }

        /**
         * Run the `prefer-destructuring` check on an AssignmentExpression
         *
         * @param {ASTNode} node the AssignmentExpression node
         * @returns {void}
         */
        function checkAssigmentExpression(node) {
            if (node.operator === "=") {
                performCheck(node.left, node.right, node);
            }
        }

        //--------------------------------------------------------------------------
        // Public
        //--------------------------------------------------------------------------

        return {
            VariableDeclarator: checkVariableDeclarator,
            AssignmentExpression: checkAssigmentExpression
        };
    }
};
