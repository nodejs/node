/**
 * @fileoverview A rule to suggest using of const declaration for variables that are never reassigned after declared.
 * @author Toru Nagashima
 * @copyright 2015 Toru Nagashima. All rights reserved.
 */

"use strict";

//------------------------------------------------------------------------------
// Requirements
//------------------------------------------------------------------------------

var Map = require("es6-map");

//------------------------------------------------------------------------------
// Helpers
//------------------------------------------------------------------------------

var PATTERN_TYPE = /^(?:.+?Pattern|RestElement|Property)$/;
var DECLARATION_HOST_TYPE = /^(?:Program|BlockStatement|SwitchCase)$/;
var DESTRUCTURING_HOST_TYPE = /^(?:VariableDeclarator|AssignmentExpression)$/;

/**
 * Adds multiple items to the tail of an array.
 *
 * @param {any[]} array - A destination to add.
 * @param {any[]} values - Items to be added.
 * @returns {void}
 */
var pushAll = Function.apply.bind(Array.prototype.push);

/**
 * Checks whether a given node is located at `ForStatement.init` or not.
 *
 * @param {ASTNode} node - A node to check.
 * @returns {boolean} `true` if the node is located at `ForStatement.init`.
 */
function isInitOfForStatement(node) {
    return node.parent.type === "ForStatement" && node.parent.init === node;
}

/**
 * Checks whether a given Identifier node becomes a VariableDeclaration or not.
 *
 * @param {ASTNode} identifier - An Identifier node to check.
 * @returns {boolean} `true` if the node can become a VariableDeclaration.
 */
function canBecomeVariableDeclaration(identifier) {
    var node = identifier.parent;

    while (PATTERN_TYPE.test(node.type)) {
        node = node.parent;
    }

    return (
        node.type === "VariableDeclarator" ||
        (
            node.type === "AssignmentExpression" &&
            node.parent.type === "ExpressionStatement" &&
            DECLARATION_HOST_TYPE.test(node.parent.parent.type)
        )
    );
}

/**
 * Gets the WriteReference of a given variable if the variable should be
 * declared as const.
 *
 * @param {escope.Variable} variable - A variable to get.
 * @returns {escope.Reference|null} The singular WriteReference or null.
 */
function getWriteReferenceIfShouldBeConst(variable) {
    if (variable.eslintUsed) {
        return null;
    }

    // Finds the singular WriteReference.
    var retv = null;
    var references = variable.references;

    for (var i = 0; i < references.length; ++i) {
        var reference = references[i];

        if (reference.isWrite()) {
            var isReassigned = Boolean(
                retv && retv.identifier !== reference.identifier
            );

            if (isReassigned) {
                return null;
            }
            retv = reference;
        }
    }

    // Checks the writer is located in the same scope and can be modified to
    // const.
    var isSameScopeAndCanBecomeVariableDeclaration = Boolean(
        retv &&
        retv.from === variable.scope &&
        canBecomeVariableDeclaration(retv.identifier)
    );

    return isSameScopeAndCanBecomeVariableDeclaration ? retv : null;
}

/**
 * Gets the VariableDeclarator/AssignmentExpression node that a given reference
 * belongs to.
 * This is used to detect a mix of reassigned and never reassigned in a
 * destructuring.
 *
 * @param {escope.Reference} reference - A reference to get.
 * @returns {ASTNode|null} A VariableDeclarator/AssignmentExpression node or
 *      null.
 */
function getDestructuringHost(reference) {
    if (!reference.isWrite()) {
        return null;
    }
    var node = reference.identifier.parent;

    while (PATTERN_TYPE.test(node.type)) {
        node = node.parent;
    }

    if (!DESTRUCTURING_HOST_TYPE.test(node.type)) {
        return null;
    }
    return node;
}

/**
 * Groups by the VariableDeclarator/AssignmentExpression node that each
 * reference of given variables belongs to.
 * This is used to detect a mix of reassigned and never reassigned in a
 * destructuring.
 *
 * @param {escope.Variable[]} variables - Variables to group by destructuring.
 * @returns {Map<ASTNode, (escope.Reference|null)[]>} Grouped references.
 */
function groupByDestructuring(variables) {
    var writersMap = new Map();

    for (var i = 0; i < variables.length; ++i) {
        var variable = variables[i];
        var references = variable.references;
        var writer = getWriteReferenceIfShouldBeConst(variable);
        var prevId = null;

        for (var j = 0; j < references.length; ++j) {
            var reference = references[j];
            var id = reference.identifier;

            // Avoid counting a reference twice or more for default values of
            // destructuring.
            if (id === prevId) {
                continue;
            }
            prevId = id;

            // Add the writer into the destructuring group.
            var group = getDestructuringHost(reference);

            if (group) {
                if (writersMap.has(group)) {
                    writersMap.get(group).push(writer);
                } else {
                    writersMap.set(group, [writer]);
                }
            }
        }
    }

    return writersMap;
}

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = function(context) {
    var options = context.options[0] || {};
    var checkingMixedDestructuring = options.destructuring !== "all";
    var variables = null;

    /**
     * Reports a given reference.
     *
     * @param {escope.Reference} reference - A reference to report.
     * @returns {void}
     */
    function report(reference) {
        var id = reference.identifier;

        context.report({
            node: id,
            message: "'{{name}}' is never reassigned, use 'const' instead.",
            data: id
        });
    }

    /**
     * Reports a given variable if the variable should be declared as const.
     *
     * @param {escope.Variable} variable - A variable to report.
     * @returns {void}
     */
    function checkVariable(variable) {
        var writer = getWriteReferenceIfShouldBeConst(variable);

        if (writer) {
            report(writer);
        }
    }

    /**
     * Reports given references if all of the reference should be declared as
     * const.
     *
     * The argument 'writers' is an array of references.
     * This reference is the result of
     * 'getWriteReferenceIfShouldBeConst(variable)', so it's nullable.
     * In simple declaration or assignment cases, the length of the array is 1.
     * In destructuring cases, the length of the array can be 2 or more.
     *
     * @param {(escope.Reference|null)[]} writers - References which are grouped
     *      by destructuring to report.
     * @returns {void}
     */
    function checkGroup(writers) {
        if (writers.every(Boolean)) {
            writers.forEach(report);
        }
    }

    return {
        "Program": function() {
            variables = [];
        },

        "Program:exit": function() {
            if (checkingMixedDestructuring) {
                variables.forEach(checkVariable);
            } else {
                groupByDestructuring(variables).forEach(checkGroup);
            }

            variables = null;
        },

        "VariableDeclaration": function(node) {
            if (node.kind === "let" && !isInitOfForStatement(node)) {
                pushAll(variables, context.getDeclaredVariables(node));
            }
        }
    };
};

module.exports.schema = [
    {
        type: "object",
        properties: {
            destructuring: {enum: ["any", "all"]}
        },
        additionalProperties: false
    }
];
