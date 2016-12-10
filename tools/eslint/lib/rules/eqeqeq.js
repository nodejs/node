/**
 * @fileoverview Rule to flag statements that use != and == instead of !== and ===
 * @author Nicholas C. Zakas
 */

"use strict";

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = {
    meta: {
        docs: {
            description: "require the use of `===` and `!==`",
            category: "Best Practices",
            recommended: false
        },

        schema: {
            anyOf: [
                {
                    type: "array",
                    items: [
                        {
                            enum: ["always"]
                        },
                        {
                            type: "object",
                            properties: {
                                null: {
                                    enum: ["always", "never", "ignore"]
                                }
                            },
                            additionalProperties: false
                        }
                    ],
                    additionalItems: false
                },
                {
                    type: "array",
                    items: [
                        {
                            enum: ["smart", "allow-null"]
                        }
                    ],
                    additionalItems: false
                }
            ]
        }
    },

    create(context) {
        const config = context.options[0] || "always";
        const options = context.options[1] || {};
        const sourceCode = context.getSourceCode();

        const nullOption = (config === "always") ?
            options.null || "always" :
            "ignore";
        const enforceRuleForNull = (nullOption === "always");
        const enforceInverseRuleForNull = (nullOption === "never");

        /**
         * Checks if an expression is a typeof expression
         * @param  {ASTNode} node The node to check
         * @returns {boolean} if the node is a typeof expression
         */
        function isTypeOf(node) {
            return node.type === "UnaryExpression" && node.operator === "typeof";
        }

        /**
         * Checks if either operand of a binary expression is a typeof operation
         * @param {ASTNode} node The node to check
         * @returns {boolean} if one of the operands is typeof
         * @private
         */
        function isTypeOfBinary(node) {
            return isTypeOf(node.left) || isTypeOf(node.right);
        }

        /**
         * Checks if operands are literals of the same type (via typeof)
         * @param {ASTNode} node The node to check
         * @returns {boolean} if operands are of same type
         * @private
         */
        function areLiteralsAndSameType(node) {
            return node.left.type === "Literal" && node.right.type === "Literal" &&
                    typeof node.left.value === typeof node.right.value;
        }

        /**
         * Checks if one of the operands is a literal null
         * @param {ASTNode} node The node to check
         * @returns {boolean} if operands are null
         * @private
         */
        function isNullCheck(node) {
            return (node.right.type === "Literal" && node.right.value === null) ||
                    (node.left.type === "Literal" && node.left.value === null);
        }

        /**
         * Gets the location (line and column) of the binary expression's operator
         * @param {ASTNode} node The binary expression node to check
         * @param {string} operator The operator to find
         * @returns {Object} { line, column } location of operator
         * @private
         */
        function getOperatorLocation(node) {
            const opToken = sourceCode.getTokenAfter(node.left);

            return {line: opToken.loc.start.line, column: opToken.loc.start.column};
        }

        /**
         * Reports a message for this rule.
         * @param {ASTNode} node The binary expression node that was checked
         * @param {string} message The message to report
         * @returns {void}
         * @private
         */
        function report(node, message) {
            context.report({
                node,
                loc: getOperatorLocation(node),
                message,
                data: { op: node.operator.charAt(0) }
            });
        }

        return {
            BinaryExpression(node) {
                const isNull = isNullCheck(node);

                if (node.operator !== "==" && node.operator !== "!=") {
                    if (enforceInverseRuleForNull && isNull) {
                        report(node, "Expected '{{op}}=' and instead saw '{{op}}=='.");
                    }
                    return;
                }

                if (config === "smart" && (isTypeOfBinary(node) ||
                        areLiteralsAndSameType(node) || isNull)) {
                    return;
                }

                if (!enforceRuleForNull && isNull) {
                    return;
                }

                report(node, "Expected '{{op}}==' and instead saw '{{op}}='.");
            }
        };

    }
};
