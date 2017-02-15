/**
 * @fileoverview Rule to flag when using constructor without parentheses
 * @author Ilya Volodin
 */

"use strict";

//------------------------------------------------------------------------------
// Helpers
//------------------------------------------------------------------------------

/**
 * Checks whether the given token is an opening parenthesis or not.
 *
 * @param {Token} token - The token to check.
 * @returns {boolean} `true` if the token is an opening parenthesis.
 */
function isOpeningParen(token) {
    return token.type === "Punctuator" && token.value === "(";
}

/**
 * Checks whether the given token is an closing parenthesis or not.
 *
 * @param {Token} token - The token to check.
 * @returns {boolean} `true` if the token is an closing parenthesis.
 */
function isClosingParen(token) {
    return token.type === "Punctuator" && token.value === ")";
}

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = {
    meta: {
        docs: {
            description: "require parentheses when invoking a constructor with no arguments",
            category: "Stylistic Issues",
            recommended: false
        },

        schema: [],

        fixable: "code"
    },

    create(context) {
        const sourceCode = context.getSourceCode();

        return {
            NewExpression(node) {
                if (node.arguments.length !== 0) {
                    return;  // shortcut: if there are arguments, there have to be parens
                }

                const lastToken = sourceCode.getLastToken(node);
                const hasLastParen = lastToken && isClosingParen(lastToken);
                const hasParens = hasLastParen && isOpeningParen(sourceCode.getTokenBefore(lastToken));

                if (!hasParens) {
                    context.report({
                        node,
                        message: "Missing '()' invoking a constructor.",
                        fix: fixer => fixer.insertTextAfter(node, "()")
                    });
                }
            }
        };
    }
};
