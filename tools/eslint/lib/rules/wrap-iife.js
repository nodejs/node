/**
 * @fileoverview Rule to flag when IIFE is not wrapped in parens
 * @author Ilya Volodin
 */

"use strict";

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = {
    meta: {
        docs: {
            description: "require parentheses around immediate `function` invocations",
            category: "Best Practices",
            recommended: false
        },

        schema: [
            {
                enum: ["outside", "inside", "any"]
            }
        ]
    },

    create(context) {

        const style = context.options[0] || "outside";

        const sourceCode = context.getSourceCode();

        /**
         * Check if the node is wrapped in ()
         * @param {ASTNode} node node to evaluate
         * @returns {boolean} True if it is wrapped
         * @private
         */
        function wrapped(node) {
            const previousToken = sourceCode.getTokenBefore(node),
                nextToken = sourceCode.getTokenAfter(node);

            return previousToken && previousToken.value === "(" &&
                nextToken && nextToken.value === ")";
        }

        return {

            CallExpression(node) {
                if (node.callee.type === "FunctionExpression") {
                    const callExpressionWrapped = wrapped(node),
                        functionExpressionWrapped = wrapped(node.callee);

                    if (!callExpressionWrapped && !functionExpressionWrapped) {
                        context.report(node, "Wrap an immediate function invocation in parentheses.");
                    } else if (style === "inside" && !functionExpressionWrapped) {
                        context.report(node, "Wrap only the function expression in parens.");
                    } else if (style === "outside" && !callExpressionWrapped) {
                        context.report(node, "Move the invocation into the parens that contain the function.");
                    }
                }
            }
        };

    }
};
