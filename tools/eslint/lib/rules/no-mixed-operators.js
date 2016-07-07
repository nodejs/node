/**
 * @fileoverview Rule to disallow mixed binary operators.
 * @author Toru Nagashima
 */

"use strict";

//------------------------------------------------------------------------------
// Requirements
//------------------------------------------------------------------------------

var astUtils = require("../ast-utils.js");

//------------------------------------------------------------------------------
// Helpers
//------------------------------------------------------------------------------

var ARITHMETIC_OPERATORS = ["+", "-", "*", "/", "%", "**"];
var BITWISE_OPERATORS = ["&", "|", "^", "~", "<<", ">>", ">>>"];
var COMPARISON_OPERATORS = ["==", "!=", "===", "!==", ">", ">=", "<", "<="];
var LOGICAL_OPERATORS = ["&&", "||"];
var RELATIONAL_OPERATORS = ["in", "instanceof"];
var ALL_OPERATORS = [].concat(
    ARITHMETIC_OPERATORS,
    BITWISE_OPERATORS,
    COMPARISON_OPERATORS,
    LOGICAL_OPERATORS,
    RELATIONAL_OPERATORS
);
var DEFAULT_GROUPS = [
    ARITHMETIC_OPERATORS,
    BITWISE_OPERATORS,
    COMPARISON_OPERATORS,
    LOGICAL_OPERATORS,
    RELATIONAL_OPERATORS
];
var TARGET_NODE_TYPE = /^(?:Binary|Logical)Expression$/;

/**
 * Normalizes options.
 *
 * @param {object|undefined} options - A options object to normalize.
 * @returns {object} Normalized option object.
 */
function normalizeOptions(options) {
    var hasGroups = (options && options.groups && options.groups.length > 0);
    var groups = hasGroups ? options.groups : DEFAULT_GROUPS;
    var allowSamePrecedence = (options && options.allowSamePrecedence) !== false;

    return {
        groups: groups,
        allowSamePrecedence: allowSamePrecedence
    };
}

/**
 * Checks whether any group which includes both given operator exists or not.
 *
 * @param {Array.<string[]>} groups - A list of groups to check.
 * @param {string} left - An operator.
 * @param {string} right - Another operator.
 * @returns {boolean} `true` if such group existed.
 */
function includesBothInAGroup(groups, left, right) {
    return groups.some(function(group) {
        return group.indexOf(left) !== -1 && group.indexOf(right) !== -1;
    });
}

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = {
    meta: {
        docs: {
            description: "disallow mixed binary operators",
            category: "Stylistic Issues",
            recommended: false
        },
        schema: [
            {
                type: "object",
                properties: {
                    groups: {
                        type: "array",
                        items: {
                            type: "array",
                            items: {enum: ALL_OPERATORS},
                            minItems: 2,
                            uniqueItems: true
                        },
                        uniqueItems: true
                    },
                    allowSamePrecedence: {
                        type: "boolean"
                    }
                },
                additionalProperties: false
            }
        ]
    },

    create: function(context) {
        var sourceCode = context.getSourceCode();
        var options = normalizeOptions(context.options[0]);

        /**
         * Checks whether a given node should be ignored by options or not.
         *
         * @param {ASTNode} node - A node to check. This is a BinaryExpression
         *      node or a LogicalExpression node. This parent node is one of
         *      them, too.
         * @returns {boolean} `true` if the node should be ignored.
         */
        function shouldIgnore(node) {
            var a = node;
            var b = node.parent;

            return (
                !includesBothInAGroup(options.groups, a.operator, b.operator) ||
                (
                    options.allowSamePrecedence &&
                    astUtils.getPrecedence(a) === astUtils.getPrecedence(b)
                )
            );
        }

        /**
         * Checks whether the operator of a given node is mixed with parent
         * node's operator or not.
         *
         * @param {ASTNode} node - A node to check. This is a BinaryExpression
         *      node or a LogicalExpression node. This parent node is one of
         *      them, too.
         * @returns {boolean} `true` if the node was mixed.
         */
        function isMixedWithParent(node) {
            return (
                node.operator !== node.parent.operator &&
                !astUtils.isParenthesised(sourceCode, node)
            );
        }

        /**
         * Gets the operator token of a given node.
         *
         * @param {ASTNode} node - A node to check. This is a BinaryExpression
         *      node or a LogicalExpression node.
         * @returns {Token} The operator token of the node.
         */
        function getOperatorToken(node) {
            var token = sourceCode.getTokenAfter(node.left);

            while (token.value === ")") {
                token = sourceCode.getTokenAfter(token);
            }

            return token;
        }

        /**
         * Reports both the operator of a given node and the operator of the
         * parent node.
         *
         * @param {ASTNode} node - A node to check. This is a BinaryExpression
         *      node or a LogicalExpression node. This parent node is one of
         *      them, too.
         * @returns {void}
         */
        function reportBothOperators(node) {
            var parent = node.parent;
            var left = (parent.left === node) ? node : parent;
            var right = (parent.left !== node) ? node : parent;
            var message =
                "Unexpected mix of '" + left.operator + "' and '" +
                right.operator + "'.";

            context.report({
                node: left,
                loc: getOperatorToken(left).loc.start,
                message: message
            });
            context.report({
                node: right,
                loc: getOperatorToken(right).loc.start,
                message: message
            });
        }

        /**
         * Checks between the operator of this node and the operator of the
         * parent node.
         *
         * @param {ASTNode} node - A node to check.
         * @returns {void}
         */
        function check(node) {
            if (TARGET_NODE_TYPE.test(node.parent.type) &&
                isMixedWithParent(node) &&
                !shouldIgnore(node)
            ) {
                reportBothOperators(node);
            }
        }

        return {
            BinaryExpression: check,
            LogicalExpression: check
        };
    }
};
