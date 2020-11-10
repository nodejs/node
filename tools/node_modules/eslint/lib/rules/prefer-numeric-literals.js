/**
 * @fileoverview Rule to disallow `parseInt()` in favor of binary, octal, and hexadecimal literals
 * @author Annie Zhang, Henry Zhu
 */

"use strict";

//------------------------------------------------------------------------------
// Requirements
//------------------------------------------------------------------------------

const astUtils = require("./utils/ast-utils");

//------------------------------------------------------------------------------
// Helpers
//------------------------------------------------------------------------------

const radixMap = new Map([
    [2, { system: "binary", literalPrefix: "0b" }],
    [8, { system: "octal", literalPrefix: "0o" }],
    [16, { system: "hexadecimal", literalPrefix: "0x" }]
]);

/**
 * Checks to see if a CallExpression's callee node is `parseInt` or
 * `Number.parseInt`.
 * @param {ASTNode} calleeNode The callee node to evaluate.
 * @returns {boolean} True if the callee is `parseInt` or `Number.parseInt`,
 * false otherwise.
 */
function isParseInt(calleeNode) {
    return (
        astUtils.isSpecificId(calleeNode, "parseInt") ||
        astUtils.isSpecificMemberAccess(calleeNode, "Number", "parseInt")
    );
}

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = {
    meta: {
        type: "suggestion",

        docs: {
            description: "disallow `parseInt()` and `Number.parseInt()` in favor of binary, octal, and hexadecimal literals",
            category: "ECMAScript 6",
            recommended: false,
            url: "https://eslint.org/docs/rules/prefer-numeric-literals"
        },

        schema: [],

        messages: {
            useLiteral: "Use {{system}} literals instead of {{functionName}}()."
        },

        fixable: "code"
    },

    create(context) {
        const sourceCode = context.getSourceCode();

        //----------------------------------------------------------------------
        // Public
        //----------------------------------------------------------------------

        return {

            "CallExpression[arguments.length=2]"(node) {
                const [strNode, radixNode] = node.arguments,
                    str = astUtils.getStaticStringValue(strNode),
                    radix = radixNode.value;

                if (
                    str !== null &&
                    astUtils.isStringLiteral(strNode) &&
                    radixNode.type === "Literal" &&
                    typeof radix === "number" &&
                    radixMap.has(radix) &&
                    isParseInt(node.callee)
                ) {

                    const { system, literalPrefix } = radixMap.get(radix);

                    context.report({
                        node,
                        messageId: "useLiteral",
                        data: {
                            system,
                            functionName: sourceCode.getText(node.callee)
                        },
                        fix(fixer) {
                            if (sourceCode.getCommentsInside(node).length) {
                                return null;
                            }

                            const replacement = `${literalPrefix}${str}`;

                            if (+replacement !== parseInt(str, radix)) {

                                /*
                                 * If the newly-produced literal would be invalid, (e.g. 0b1234),
                                 * or it would yield an incorrect parseInt result for some other reason, don't make a fix.
                                 */
                                return null;
                            }

                            const tokenBefore = sourceCode.getTokenBefore(node),
                                tokenAfter = sourceCode.getTokenAfter(node);
                            let prefix = "",
                                suffix = "";

                            if (
                                tokenBefore &&
                                tokenBefore.range[1] === node.range[0] &&
                                !astUtils.canTokensBeAdjacent(tokenBefore, replacement)
                            ) {
                                prefix = " ";
                            }

                            if (
                                tokenAfter &&
                                node.range[1] === tokenAfter.range[0] &&
                                !astUtils.canTokensBeAdjacent(replacement, tokenAfter)
                            ) {
                                suffix = " ";
                            }

                            return fixer.replaceText(node, `${prefix}${replacement}${suffix}`);
                        }
                    });
                }
            }
        };
    }
};
