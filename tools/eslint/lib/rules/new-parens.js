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

/**
 * Checks whether the given node is inside of another given node.
 *
 * @param {ASTNode|Token} inner - The inner node to check.
 * @param {ASTNode|Token} outer - The outer node to check.
 * @returns {boolean} `true` if the `inner` is in `outer`.
 */
function isInRange(inner, outer) {
    const ir = inner.range;
    const or = outer.range;

    return or[0] <= ir[0] && ir[1] <= or[1];
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
                let token = sourceCode.getTokenAfter(node.callee);

                // Skip ')'
                while (token && isClosingParen(token)) {
                    token = sourceCode.getTokenAfter(token);
                }

                if (!(token && isOpeningParen(token) && isInRange(token, node))) {
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
