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

        schema: [
            {
                enum: ["always", "smart", "allow-null"]
            }
        ]
    },

    create: function(context) {
        let sourceCode = context.getSourceCode();

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
            let opToken = sourceCode.getTokenAfter(node.left);

            return {line: opToken.loc.start.line, column: opToken.loc.start.column};
        }

        return {
            BinaryExpression: function(node) {
                if (node.operator !== "==" && node.operator !== "!=") {
                    return;
                }

                if (context.options[0] === "smart" && (isTypeOfBinary(node) ||
                        areLiteralsAndSameType(node) || isNullCheck(node))) {
                    return;
                }

                if (context.options[0] === "allow-null" && isNullCheck(node)) {
                    return;
                }

                context.report({
                    node: node,
                    loc: getOperatorLocation(node),
                    message: "Expected '{{op}}=' and instead saw '{{op}}'.",
                    data: { op: node.operator }
                });

            }
        };

    }
};
