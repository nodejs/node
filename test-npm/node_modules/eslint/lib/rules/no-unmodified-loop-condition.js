/**
 * @fileoverview Rule to disallow use of unmodified expressions in loop conditions
 * @author Toru Nagashima
 * @copyright 2015 Toru Nagashima. All rights reserved.
 * See LICENSE file in root directory for full license.
 */

"use strict";

//------------------------------------------------------------------------------
// Requirements
//------------------------------------------------------------------------------

var Map = require("es6-map"),
    estraverse = require("../util/estraverse"),
    astUtils = require("../ast-utils");

//------------------------------------------------------------------------------
// Helpers
//------------------------------------------------------------------------------

var pushAll = Function.apply.bind(Array.prototype.push);
var SENTINEL_PATTERN = /(?:(?:Call|Class|Function|Member|New|Yield)Expression|Statement|Declaration)$/;
var LOOP_PATTERN = /^(?:DoWhile|For|While)Statement$/;
var GROUP_PATTERN = /^(?:BinaryExpression|ConditionalExpression)$/;
var SKIP_PATTERN = /^(?:ArrowFunction|Class|Function)Expression$/;
var DYNAMIC_PATTERN = /^(?:Call|Member|New|TaggedTemplate|Yield)Expression$/;

/**
 * @typedef {object} LoopConditionInfo
 * @property {escope.Reference} reference - The reference.
 * @property {ASTNode[]} groups - BinaryExpression or ConditionalExpression
 *      nodes that the reference is belonging to.
 * @property {function} isInLoop - The predicate which checks a given reference
 *      is in this loop.
 * @property {boolean} modified - The flag that the reference is modified in
 *      this loop.
 */

/**
 * Checks whether or not a given reference is a write reference.
 *
 * @param {escope.Reference} reference - A reference to check.
 * @returns {boolean} `true` if the reference is a write reference.
 */
function isWriteReference(reference) {
    if (reference.init) {
        var def = reference.resolved && reference.resolved.defs[0];
        if (!def || def.type !== "Variable" || def.parent.kind !== "var") {
            return false;
        }
    }
    return reference.isWrite();
}

/**
 * Checks whether or not a given loop condition info does not have the modified
 * flag.
 *
 * @param {LoopConditionInfo} condition - A loop condition info to check.
 * @returns {boolean} `true` if the loop condition info is "unmodified".
 */
function isUnmodified(condition) {
    return !condition.modified;
}

/**
 * Checks whether or not a given loop condition info does not have the modified
 * flag and does not have any groups that this condition is belonging to.
 *
 * @param {LoopConditionInfo} condition - A loop condition info to check.
 * @returns {boolean} `true` if the loop condition info is "unmodified".
 */
function isUnmodifiedAndNotBelongToGroup(condition) {
    return !condition.modified && condition.groups.length === 0;
}

/**
 * Checks whether or not a given reference is inside of a given node.
 *
 * @param {ASTNode} node - A node to check.
 * @param {escope.Reference} reference - A reference to check.
 * @returns {boolean} `true` if the reference is inside of the node.
 */
function isInRange(node, reference) {
    var or = node.range;
    var ir = reference.identifier.range;
    return or[0] <= ir[0] && ir[1] <= or[1];
}

/**
 * Checks whether or not a given reference is inside of a loop node's condition.
 *
 * @param {ASTNode} node - A node to check.
 * @param {escope.Reference} reference - A reference to check.
 * @returns {boolean} `true` if the reference is inside of the loop node's
 *      condition.
 */
var isInLoop = {
    WhileStatement: isInRange,
    DoWhileStatement: isInRange,
    ForStatement: function(node, reference) {
        return (
            isInRange(node, reference) &&
            !(node.init && isInRange(node.init, reference))
        );
    }
};

/**
 * Checks whether or not a given group node has any dynamic elements.
 *
 * @param {ASTNode} root - A node to check.
 *      This node is one of BinaryExpression or ConditionalExpression.
 * @returns {boolean} `true` if the node is dynamic.
 */
function hasDynamicExpressions(root) {
    var retv = false;

    estraverse.traverse(root, {
        enter: function(node) {
            if (DYNAMIC_PATTERN.test(node.type)) {
                retv = true;
                this.break();
            } else if (SKIP_PATTERN.test(node.type)) {
                this.skip();
            }
        }
    });

    return retv;
}

/**
 * Creates the loop condition information from a given reference.
 *
 * @param {escope.Reference} reference - A reference to create.
 * @returns {LoopConditionInfo|null} Created loop condition info, or null.
 */
function toLoopCondition(reference) {
    if (reference.init) {
        return null;
    }

    var groups = [];
    var child = reference.identifier;
    var node = child.parent;
    while (node) {
        if (SENTINEL_PATTERN.test(node.type)) {
            if (LOOP_PATTERN.test(node.type) && node.test === child) {
                // This reference is inside of a loop condition.
                return {
                    reference: reference,
                    groups: groups,
                    isInLoop: isInLoop[node.type].bind(null, node),
                    modified: false
                };
            }
            // This reference is outside of a loop condition.
            break;
        }

        // If it's inside of a group, OK if either operand is modified.
        // So stores the group that the reference is belonging to.
        if (GROUP_PATTERN.test(node.type)) {
            if (hasDynamicExpressions(node)) {
                // This expression is dynamic, so don't check.
                break;
            } else {
                groups.push(node);
            }
        }

        child = node;
        node = node.parent;
    }

    return null;
}

/**
 * Gets the function which encloses a given reference.
 * This supports only FunctionDeclaration.
 *
 * @param {escope.Reference} reference - A reference to get.
 * @returns {ASTNode|null} The function node or null.
 */
function getEncloseFunctionDeclaration(reference) {
    var node = reference.identifier;

    while (node) {
        if (node.type === "FunctionDeclaration") {
            return node.id ? node : null;
        }

        node = node.parent;
    }

    return null;
}

/**
 * Updates the "modified" flags of given loop conditions with given modifiers.
 *
 * @param {LoopConditionInfo[]} conditions - The loop conditions to be updated.
 * @param {escope.Reference[]} modifiers - The references to update.
 * @returns {void}
 */
function updateModifiedFlag(conditions, modifiers) {
    var funcNode, funcVar;

    for (var i = 0; i < conditions.length; ++i) {
        var condition = conditions[i];

        for (var j = 0; !condition.modified && j < modifiers.length; ++j) {
            var modifier = modifiers[j];
            var inLoop = condition.isInLoop(modifier) || Boolean(
                // Checks the function that this modifier is belonging to is used in the loop.
                (funcNode = getEncloseFunctionDeclaration(modifier)) &&
                (funcVar = astUtils.getVariableByName(modifier.from.upper, funcNode.id.name)) &&
                funcVar.references.some(condition.isInLoop)
            );
            condition.modified = inLoop;
        }
    }
}

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = function(context) {
    var groupMap = null;

    /**
     * Reports a given condition info.
     *
     * @param {LoopConditionInfo} condition - A loop condition info to report.
     * @returns {void}
     */
    function report(condition) {
        var node = condition.reference.identifier;

        context.report({
            node: node,
            message: "'{{name}}' is not modified in this loop.",
            data: node
        });
    }

    /**
     * Registers given conditions to groups that the condition is belonging to.
     *
     * @param {LoopConditionInfo[]} conditions - A loop condition info to
     *      register.
     * @returns {void}
     */
    function registerConditionsToGroup(conditions) {
        for (var i = 0; i < conditions.length; ++i) {
            var condition = conditions[i];

            for (var j = 0; j < condition.groups.length; ++j) {
                var be = condition.groups[j];
                var group = groupMap.get(be);
                if (!group) {
                    group = [];
                    groupMap.set(be, group);
                }
                group.push(condition);
            }
        }
    }

    /**
     * Reports references which are inside of unmodified groups.
     *
     * @param {LoopConditionInfo[]} conditions - A loop condition info to report.
     * @returns {void}
     */
    function checkConditionsInGroup(conditions) {
        if (conditions.every(isUnmodified)) {
            conditions.forEach(report);
        }
    }

    /**
     * Finds unmodified references which are inside of a loop condition.
     * Then reports the references which are outside of groups.
     *
     * @param {escope.Variable} variable - A variable to report.
     * @returns {void}
     */
    function checkReferences(variable) {
        // Gets references that exist in loop conditions.
        var conditions = variable
            .references
            .map(toLoopCondition)
            .filter(Boolean);

        if (conditions.length === 0) {
            return;
        }

        // Registers the conditions to belonging groups.
        registerConditionsToGroup(conditions);

        // Check the conditions are modified.
        var modifiers = variable.references.filter(isWriteReference);
        if (modifiers.length > 0) {
            updateModifiedFlag(conditions, modifiers);
        }

        // Reports the conditions which are not belonging to groups.
        // Others will be reported after all variables are done.
        conditions
            .filter(isUnmodifiedAndNotBelongToGroup)
            .forEach(report);
    }

    return {
        "Program:exit": function() {
            var queue = [context.getScope()];
            groupMap = new Map();

            var scope;
            while ((scope = queue.pop())) {
                pushAll(queue, scope.childScopes);
                scope.variables.forEach(checkReferences);
            }

            groupMap.forEach(checkConditionsInGroup);
            groupMap = null;
        }
    };
};

module.exports.schema = [];
