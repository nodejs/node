/**
 * @fileoverview Enforce newlines between operands of ternary expressions
 * @author Kai Cataldo
 */

"use strict";

let astUtils = require("../ast-utils");

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = {
    meta: {
        docs: {
            description: "enforce newlines between operands of ternary expressions",
            category: "Stylistic Issues",
            recommended: false
        },
        schema: []
    },

    create: function(context) {

        //--------------------------------------------------------------------------
        // Helpers
        //--------------------------------------------------------------------------

        /**
         * Tests whether node is preceded by supplied tokens
         * @param {ASTNode} node - node to check
         * @param {ASTNode} parentNode - parent of node to report
         * @returns {void}
         * @private
         */
        function reportError(node, parentNode) {
            context.report({
                node: node,
                message: "Expected newline between {{typeOfError}} of ternary expression.",
                data: {
                    typeOfError: node === parentNode.test ? "test and consequent" : "consequent and alternate"
                }
            });
        }

        //--------------------------------------------------------------------------
        // Public
        //--------------------------------------------------------------------------

        return {
            ConditionalExpression: function(node) {
                let areTestAndConsequentOnSameLine = astUtils.isTokenOnSameLine(node.test, node.consequent);
                let areConsequentAndAlternateOnSameLine = astUtils.isTokenOnSameLine(node.consequent, node.alternate);

                if (areTestAndConsequentOnSameLine) {
                    reportError(node.test, node);
                }

                if (areConsequentAndAlternateOnSameLine) {
                    reportError(node.consequent, node);
                }
            }
        };
    }
};
