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
         * @param {ASTNode} obj The object owning the property.
         * @param {ASTNode} prop The property of the object.
         * @param {ASTNode} node The corresponding node of the token.
         * @returns {void}
         */
        function checkDotLocation(obj, prop, node) {
            const dot = sourceCode.getTokenBefore(prop);

            // `obj` expression can be parenthesized, but those paren tokens are not a part of the `obj` node.
            const tokenBeforeDot = sourceCode.getTokenBefore(dot);

            const textBeforeDot = sourceCode.getText().slice(tokenBeforeDot.range[1], dot.range[0]);
            const textAfterDot = sourceCode.getText().slice(dot.range[1], prop.range[0]);

            if (onObject) {
                if (!astUtils.isTokenOnSameLine(tokenBeforeDot, dot)) {
                    const neededTextAfterToken = astUtils.isDecimalIntegerNumericToken(tokenBeforeDot) ? " " : "";

                    context.report({
                        node,
                        loc: dot.loc.start,
                        messageId: "expectedDotAfterObject",
                        fix: fixer => fixer.replaceTextRange([tokenBeforeDot.range[1], prop.range[0]], `${neededTextAfterToken}.${textBeforeDot}${textAfterDot}`)
                    });
                }
            } else if (!astUtils.isTokenOnSameLine(dot, prop)) {
                context.report({
                    node,
                    loc: dot.loc.start,
                    messageId: "expectedDotBeforeProperty",
                    fix: fixer => fixer.replaceTextRange([tokenBeforeDot.range[1], prop.range[0]], `${textBeforeDot}${textAfterDot}.`)
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
                checkDotLocation(node.object, node.property, node);
            }
        }

        return {
            MemberExpression: checkNode
        };
    }
};
