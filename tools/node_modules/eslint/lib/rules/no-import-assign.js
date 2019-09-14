/**
 * @fileoverview Rule to flag updates of imported bindings.
 * @author Toru Nagashima <https://github.com/mysticatea>
 */

"use strict";

//------------------------------------------------------------------------------
// Helpers
//------------------------------------------------------------------------------

const { findVariable, getPropertyName } = require("eslint-utils");

const MutationMethods = {
    Object: new Set([
        "assign", "defineProperties", "defineProperty", "freeze",
        "setPrototypeOf"
    ]),
    Reflect: new Set([
        "defineProperty", "deleteProperty", "set", "setPrototypeOf"
    ])
};

/**
 * Check if a given node is LHS of an assignment node.
 * @param {ASTNode} node The node to check.
 * @returns {boolean} `true` if the node is LHS.
 */
function isAssignmentLeft(node) {
    const { parent } = node;

    return (
        (
            parent.type === "AssignmentExpression" &&
            parent.left === node
        ) ||

        // Destructuring assignments
        parent.type === "ArrayPattern" ||
        (
            parent.type === "Property" &&
            parent.value === node &&
            parent.parent.type === "ObjectPattern"
        ) ||
        parent.type === "RestElement" ||
        (
            parent.type === "AssignmentPattern" &&
            parent.left === node
        )
    );
}

/**
 * Check if a given node is the operand of mutation unary operator.
 * @param {ASTNode} node The node to check.
 * @returns {boolean} `true` if the node is the operand of mutation unary operator.
 */
function isOperandOfMutationUnaryOperator(node) {
    const { parent } = node;

    return (
        (
            parent.type === "UpdateExpression" &&
            parent.argument === node
        ) ||
        (
            parent.type === "UnaryExpression" &&
            parent.operator === "delete" &&
            parent.argument === node
        )
    );
}

/**
 * Check if a given node is the iteration variable of `for-in`/`for-of` syntax.
 * @param {ASTNode} node The node to check.
 * @returns {boolean} `true` if the node is the iteration variable.
 */
function isIterationVariable(node) {
    const { parent } = node;

    return (
        (
            parent.type === "ForInStatement" &&
            parent.left === node
        ) ||
        (
            parent.type === "ForOfStatement" &&
            parent.left === node
        )
    );
}

/**
 * Check if a given node is the iteration variable of `for-in`/`for-of` syntax.
 * @param {ASTNode} node The node to check.
 * @param {Scope} scope A `escope.Scope` object to find variable (whichever).
 * @returns {boolean} `true` if the node is the iteration variable.
 */
function isArgumentOfWellKnownMutationFunction(node, scope) {
    const { parent } = node;

    if (
        parent.type === "CallExpression" &&
        parent.arguments[0] === node &&
        parent.callee.type === "MemberExpression" &&
        parent.callee.object.type === "Identifier"
    ) {
        const { callee } = parent;
        const { object } = callee;

        if (Object.keys(MutationMethods).includes(object.name)) {
            const variable = findVariable(scope, object);

            return (
                variable !== null &&
                variable.scope.type === "global" &&
                MutationMethods[object.name].has(getPropertyName(callee, scope))
            );
        }
    }

    return false;
}

/**
 * Check if the identifier node is placed at to update members.
 * @param {ASTNode} id The Identifier node to check.
 * @param {Scope} scope A `escope.Scope` object to find variable (whichever).
 * @returns {boolean} `true` if the member of `id` was updated.
 */
function isMemberWrite(id, scope) {
    const { parent } = id;

    return (
        (
            parent.type === "MemberExpression" &&
            parent.object === id &&
            (
                isAssignmentLeft(parent) ||
                isOperandOfMutationUnaryOperator(parent) ||
                isIterationVariable(parent)
            )
        ) ||
        isArgumentOfWellKnownMutationFunction(id, scope)
    );
}

/**
 * Get the mutation node.
 * @param {ASTNode} id The Identifier node to get.
 * @returns {ASTNode} The mutation node.
 */
function getWriteNode(id) {
    let node = id.parent;

    while (
        node &&
        node.type !== "AssignmentExpression" &&
        node.type !== "UpdateExpression" &&
        node.type !== "UnaryExpression" &&
        node.type !== "CallExpression" &&
        node.type !== "ForInStatement" &&
        node.type !== "ForOfStatement"
    ) {
        node = node.parent;
    }

    return node || id;
}

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = {
    meta: {
        type: "problem",

        docs: {
            description: "disallow assigning to imported bindings",
            category: "Possible Errors",
            recommended: false,
            url: "https://eslint.org/docs/rules/no-import-assign"
        },

        schema: [],

        messages: {
            readonly: "'{{name}}' is read-only.",
            readonlyMember: "The members of '{{name}}' are read-only."
        }
    },

    create(context) {
        return {
            ImportDeclaration(node) {
                const scope = context.getScope();

                for (const variable of context.getDeclaredVariables(node)) {
                    const shouldCheckMembers = variable.defs.some(
                        d => d.node.type === "ImportNamespaceSpecifier"
                    );
                    let prevIdNode = null;

                    for (const reference of variable.references) {
                        const idNode = reference.identifier;

                        /*
                         * AssignmentPattern (e.g. `[a = 0] = b`) makes two write
                         * references for the same identifier. This should skip
                         * the one of the two in order to prevent redundant reports.
                         */
                        if (idNode === prevIdNode) {
                            continue;
                        }
                        prevIdNode = idNode;

                        if (reference.isWrite()) {
                            context.report({
                                node: getWriteNode(idNode),
                                messageId: "readonly",
                                data: { name: idNode.name }
                            });
                        } else if (shouldCheckMembers && isMemberWrite(idNode, scope)) {
                            context.report({
                                node: getWriteNode(idNode),
                                messageId: "readonlyMember",
                                data: { name: idNode.name }
                            });
                        }
                    }
                }
            }
        };

    }
};
