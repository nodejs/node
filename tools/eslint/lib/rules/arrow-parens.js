/**
 * @fileoverview Rule to require parens in arrow function arguments.
 * @author Jxck
 */
"use strict";

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = {
    meta: {
        docs: {
            description: "require parentheses around arrow function arguments",
            category: "ECMAScript 6",
            recommended: false
        },

        schema: [
            {
                enum: ["always", "as-needed"]
            }
        ]
    },

    create: function(context) {
        var message = "Expected parentheses around arrow function argument.";
        var asNeededMessage = "Unexpected parentheses around single function argument";
        var asNeeded = context.options[0] === "as-needed";

        /**
         * Determines whether a arrow function argument end with `)`
         * @param {ASTNode} node The arrow function node.
         * @returns {void}
         */
        function parens(node) {
            var token = context.getFirstToken(node);

            // as-needed: x => x
            if (asNeeded && node.params.length === 1 && node.params[0].type === "Identifier") {
                if (token.type === "Punctuator" && token.value === "(") {
                    context.report(node, asNeededMessage);
                }
                return;
            }

            if (token.type === "Identifier") {
                var after = context.getTokenAfter(token);

                // (x) => x
                if (after.value !== ")") {
                    context.report(node, message);
                }
            }
        }

        return {
            ArrowFunctionExpression: parens
        };
    }
};
