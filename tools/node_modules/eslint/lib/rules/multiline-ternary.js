/**
 * @fileoverview Enforce newlines between operands of ternary expressions
 * @author Kai Cataldo
 */

"use strict";

const astUtils = require("./utils/ast-utils");

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = {
    meta: {
        type: "layout",

        docs: {
            description: "enforce newlines between operands of ternary expressions",
            category: "Stylistic Issues",
            recommended: false,
            url: "https://eslint.org/docs/rules/multiline-ternary"
        },

        schema: [
            {
                enum: ["always", "always-multiline", "never"]
            }
        ],
        messages: {
            expectedTestCons: "Expected newline between test and consequent of ternary expression.",
            expectedConsAlt: "Expected newline between consequent and alternate of ternary expression.",
            unexpectedTestCons: "Unexpected newline between test and consequent of ternary expression.",
            unexpectedConsAlt: "Unexpected newline between consequent and alternate of ternary expression."
        }
    },

    create(context) {
        const option = context.options[0];
        const multiline = option !== "never";
        const allowSingleLine = option === "always-multiline";
        const sourceCode = context.getSourceCode();

        //--------------------------------------------------------------------------
        // Public
        //--------------------------------------------------------------------------

        return {
            ConditionalExpression(node) {
                const questionToken = sourceCode.getTokenAfter(node.test, astUtils.isNotClosingParenToken);
                const colonToken = sourceCode.getTokenAfter(node.consequent, astUtils.isNotClosingParenToken);

                const firstTokenOfTest = sourceCode.getFirstToken(node);
                const lastTokenOfTest = sourceCode.getTokenBefore(questionToken);
                const firstTokenOfConsequent = sourceCode.getTokenAfter(questionToken);
                const lastTokenOfConsequent = sourceCode.getTokenBefore(colonToken);
                const firstTokenOfAlternate = sourceCode.getTokenAfter(colonToken);

                const areTestAndConsequentOnSameLine = astUtils.isTokenOnSameLine(lastTokenOfTest, firstTokenOfConsequent);
                const areConsequentAndAlternateOnSameLine = astUtils.isTokenOnSameLine(lastTokenOfConsequent, firstTokenOfAlternate);

                if (!multiline) {
                    if (!areTestAndConsequentOnSameLine) {
                        context.report({
                            node: node.test,
                            loc: {
                                start: firstTokenOfTest.loc.start,
                                end: lastTokenOfTest.loc.end
                            },
                            messageId: "unexpectedTestCons"
                        });
                    }

                    if (!areConsequentAndAlternateOnSameLine) {
                        context.report({
                            node: node.consequent,
                            loc: {
                                start: firstTokenOfConsequent.loc.start,
                                end: lastTokenOfConsequent.loc.end
                            },
                            messageId: "unexpectedConsAlt"
                        });
                    }
                } else {
                    if (allowSingleLine && node.loc.start.line === node.loc.end.line) {
                        return;
                    }

                    if (areTestAndConsequentOnSameLine) {
                        context.report({
                            node: node.test,
                            loc: {
                                start: firstTokenOfTest.loc.start,
                                end: lastTokenOfTest.loc.end
                            },
                            messageId: "expectedTestCons"
                        });
                    }

                    if (areConsequentAndAlternateOnSameLine) {
                        context.report({
                            node: node.consequent,
                            loc: {
                                start: firstTokenOfConsequent.loc.start,
                                end: lastTokenOfConsequent.loc.end
                            },
                            messageId: "expectedConsAlt"
                        });
                    }
                }
            }
        };
    }
};
