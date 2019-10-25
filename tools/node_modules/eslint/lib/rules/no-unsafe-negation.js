/**
 * @fileoverview Rule to disallow negating the left operand of relational operators
 * @author Toru Nagashima
 */

"use strict";

//------------------------------------------------------------------------------
// Requirements
//------------------------------------------------------------------------------

const astUtils = require("./utils/ast-utils");

//------------------------------------------------------------------------------
// Helpers
//------------------------------------------------------------------------------

/**
 * Checks whether the given operator is `in` or `instanceof`
 * @param {string} op The operator type to check.
 * @returns {boolean} `true` if the operator is `in` or `instanceof`
 */
function isInOrInstanceOfOperator(op) {
    return op === "in" || op === "instanceof";
}

/**
 * Checks whether the given operator is an ordering relational operator or not.
 * @param {string} op The operator type to check.
 * @returns {boolean} `true` if the operator is an ordering relational operator.
 */
function isOrderingRelationalOperator(op) {
    return op === "<" || op === ">" || op === ">=" || op === "<=";
}

/**
 * Checks whether the given node is a logical negation expression or not.
 * @param {ASTNode} node The node to check.
 * @returns {boolean} `true` if the node is a logical negation expression.
 */
function isNegation(node) {
    return node.type === "UnaryExpression" && node.operator === "!";
}

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = {
    meta: {
        type: "problem",

        docs: {
            description: "disallow negating the left operand of relational operators",
            category: "Possible Errors",
            recommended: true,
            url: "https://eslint.org/docs/rules/no-unsafe-negation"
        },

        schema: [
            {
                type: "object",
                properties: {
                    enforceForOrderingRelations: {
                        type: "boolean",
                        default: false
                    }
                },
                additionalProperties: false
            }
        ],
        fixable: null,
        messages: {
            unexpected: "Unexpected negating the left operand of '{{operator}}' operator."
        }
    },

    create(context) {
        const sourceCode = context.getSourceCode();
        const options = context.options[0] || {};
        const enforceForOrderingRelations = options.enforceForOrderingRelations === true;

        return {
            BinaryExpression(node) {
                const orderingRelationRuleApplies = enforceForOrderingRelations && isOrderingRelationalOperator(node.operator);

                if (
                    (isInOrInstanceOfOperator(node.operator) || orderingRelationRuleApplies) &&
                    isNegation(node.left) &&
                    !astUtils.isParenthesised(sourceCode, node.left)
                ) {
                    context.report({
                        node,
                        loc: node.left.loc,
                        messageId: "unexpected",
                        data: { operator: node.operator }
                    });
                }
            }
        };
    }
};
