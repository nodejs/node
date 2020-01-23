/**
 * @fileoverview Validates newlines before and after dots
 * @author Greg Cochard
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
            description: "enforce consistent newlines before and after dots",
            category: "Best Practices",
            recommended: false,
            url: "https://eslint.org/docs/rules/dot-location"
        },

        schema: [
            {
                enum: ["object", "property"]
            }
        ],

        fixable: "code",

        messages: {
            expectedDotAfterObject: "Expected dot to be on same line as object.",
            expectedDotBeforeProperty: "Expected dot to be on same line as property."
        }
    },

    create(context) {

        const config = context.options[0];

        // default to onObject if no preference is passed
        const onObject = config === "object" || !config;

        const sourceCode = context.getSourceCode();

        /**
         * Reports if the dot between object and property is on the correct loccation.
         * @param {ASTNode} node The `MemberExpression` node.
         * @returns {void}
         */
        function checkDotLocation(node) {
            const property = node.property;
            const dot = sourceCode.getTokenBefore(property);

            // `obj` expression can be parenthesized, but those paren tokens are not a part of the `obj` node.
            const tokenBeforeDot = sourceCode.getTokenBefore(dot);

            const textBeforeDot = sourceCode.getText().slice(tokenBeforeDot.range[1], dot.range[0]);
            const textAfterDot = sourceCode.getText().slice(dot.range[1], property.range[0]);

            if (onObject) {
                if (!astUtils.isTokenOnSameLine(tokenBeforeDot, dot)) {
                    const neededTextAfterToken = astUtils.isDecimalIntegerNumericToken(tokenBeforeDot) ? " " : "";

                    context.report({
                        node,
                        loc: dot.loc,
                        messageId: "expectedDotAfterObject",
                        fix: fixer => fixer.replaceTextRange([tokenBeforeDot.range[1], property.range[0]], `${neededTextAfterToken}.${textBeforeDot}${textAfterDot}`)
                    });
                }
            } else if (!astUtils.isTokenOnSameLine(dot, property)) {
                context.report({
                    node,
                    loc: dot.loc,
                    messageId: "expectedDotBeforeProperty",
                    fix: fixer => fixer.replaceTextRange([tokenBeforeDot.range[1], property.range[0]], `${textBeforeDot}${textAfterDot}.`)
                });
            }
        }

        /**
         * Checks the spacing of the dot within a member expression.
         * @param {ASTNode} node The node to check.
         * @returns {void}
         */
        function checkNode(node) {
            if (!node.computed) {
                checkDotLocation(node);
            }
        }

        return {
            MemberExpression: checkNode
        };
    }
};
