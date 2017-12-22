/**
 * @fileoverview Rule to check empty newline between class members
 * @author 薛定谔的猫<hh_2013@foxmail.com>
 */
"use strict";

const astUtils = require("../ast-utils");

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = {
    meta: {
        docs: {
            description: "require or disallow an empty line between class members",
            category: "Stylistic Issues",
            recommended: false
        },

        fixable: "whitespace",

        schema: [
            {
                enum: ["always", "never"]
            },
            {
                type: "object",
                properties: {
                    exceptAfterSingleLine: {
                        type: "boolean"
                    }
                },
                additionalProperties: false
            }
        ]
    },

    create(context) {

        const options = [];

        options[0] = context.options[0] || "always";
        options[1] = context.options[1] || { exceptAfterSingleLine: false };

        const ALWAYS_MESSAGE = "Expected blank line between class members.";
        const NEVER_MESSAGE = "Unexpected blank line between class members.";

        const sourceCode = context.getSourceCode();

        /**
         * Checks if there is padding between two tokens
         * @param {Token} first The first token
         * @param {Token} second The second token
         * @returns {boolean} True if there is at least a line between the tokens
         */
        function isPaddingBetweenTokens(first, second) {
            return second.loc.start.line - first.loc.end.line >= 2;
        }

        return {
            ClassBody(node) {
                const body = node.body;

                for (let i = 0; i < body.length - 1; i++) {
                    const curFirst = sourceCode.getFirstToken(body[i]);
                    const curLast = sourceCode.getLastToken(body[i]);
                    const comments = sourceCode.getCommentsBefore(body[i + 1]);
                    const nextFirst = comments.length ? comments[0] : sourceCode.getFirstToken(body[i + 1]);
                    const isPadded = isPaddingBetweenTokens(curLast, nextFirst);
                    const isMulti = !astUtils.isTokenOnSameLine(curFirst, curLast);
                    const skip = !isMulti && options[1].exceptAfterSingleLine;


                    if ((options[0] === "always" && !skip && !isPadded) ||
                        (options[0] === "never" && isPadded)) {
                        context.report({
                            node: body[i + 1],
                            message: isPadded ? NEVER_MESSAGE : ALWAYS_MESSAGE,
                            fix(fixer) {
                                return isPadded
                                    ? fixer.replaceTextRange([curLast.range[1], nextFirst.range[0]], "\n")
                                    : fixer.insertTextAfter(curLast, "\n");
                            }
                        });
                    }
                }
            }
        };
    }
};
