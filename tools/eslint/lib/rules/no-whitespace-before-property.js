/**
 * @fileoverview Rule to disallow whitespace before properties
 * @author Kai Cataldo
 */
"use strict";

const astUtils = require("../ast-utils");

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = {
    meta: {
        docs: {
            description: "disallow whitespace before properties",
            category: "Stylistic Issues",
            recommended: false
        },

        fixable: "whitespace",
        schema: []
    },

    create(context) {
        const sourceCode = context.getSourceCode();

        //--------------------------------------------------------------------------
        // Helpers
        //--------------------------------------------------------------------------

        /**
         * Finds opening bracket token of node's computed property
         * @param {ASTNode} node - the node to check
         * @returns {Token} opening bracket token of node's computed property
         * @private
         */
        function findOpeningBracket(node) {
            let token = sourceCode.getTokenBefore(node.property);

            while (token.value !== "[") {
                token = sourceCode.getTokenBefore(token);
            }
            return token;
        }

        /**
         * Reports whitespace before property token
         * @param {ASTNode} node - the node to report in the event of an error
         * @param {Token} leftToken - the left token
         * @param {Token} rightToken - the right token
         * @returns {void}
         * @private
         */
        function reportError(node, leftToken, rightToken) {
            const replacementText = node.computed ? "" : ".";

            context.report({
                node,
                message: "Unexpected whitespace before property {{propName}}.",
                data: {
                    propName: sourceCode.getText(node.property)
                },
                fix(fixer) {
                    return fixer.replaceTextRange([leftToken.range[1], rightToken.range[0]], replacementText);
                }
            });
        }

        //--------------------------------------------------------------------------
        // Public
        //--------------------------------------------------------------------------

        return {
            MemberExpression(node) {
                let rightToken;
                let leftToken;

                if (!astUtils.isTokenOnSameLine(node.object, node.property)) {
                    return;
                }

                if (node.computed) {
                    rightToken = findOpeningBracket(node);
                    leftToken = sourceCode.getTokenBefore(rightToken);
                } else {
                    rightToken = sourceCode.getFirstToken(node.property);
                    leftToken = sourceCode.getTokenBefore(rightToken, 1);
                }

                if (sourceCode.isSpaceBetweenTokens(leftToken, rightToken)) {
                    reportError(node, leftToken, rightToken);
                }
            }
        };
    }
};
