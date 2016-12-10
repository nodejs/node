/**
 * @fileoverview Rule to flag no-unneeded-ternary
 * @author Gyandeep Singh
 */

"use strict";

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = {
    meta: {
        docs: {
            description: "disallow ternary operators when simpler alternatives exist",
            category: "Stylistic Issues",
            recommended: false
        },

        schema: [
            {
                type: "object",
                properties: {
                    defaultAssignment: {
                        type: "boolean"
                    }
                },
                additionalProperties: false
            }
        ]
    },

    create(context) {
        const options = context.options[0] || {};
        const defaultAssignment = options.defaultAssignment !== false;

        /**
         * Test if the node is a boolean literal
         * @param {ASTNode} node - The node to report.
         * @returns {boolean} True if the its a boolean literal
         * @private
         */
        function isBooleanLiteral(node) {
            return node.type === "Literal" && typeof node.value === "boolean";
        }

        /**
         * Test if the node matches the pattern id ? id : expression
         * @param {ASTNode} node - The ConditionalExpression to check.
         * @returns {boolean} True if the pattern is matched, and false otherwise
         * @private
         */
        function matchesDefaultAssignment(node) {
            return node.test.type === "Identifier" &&
                   node.consequent.type === "Identifier" &&
                   node.test.name === node.consequent.name;
        }

        return {

            ConditionalExpression(node) {
                if (isBooleanLiteral(node.alternate) && isBooleanLiteral(node.consequent)) {
                    context.report(node, node.consequent.loc.start, "Unnecessary use of boolean literals in conditional expression.");
                } else if (!defaultAssignment && matchesDefaultAssignment(node)) {
                    context.report(node, node.consequent.loc.start, "Unnecessary use of conditional expression for default assignment.");
                }
            }
        };
    }
};
