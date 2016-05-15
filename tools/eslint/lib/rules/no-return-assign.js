/**
 * @fileoverview Rule to flag when return statement contains assignment
 * @author Ilya Volodin
 */
"use strict";

//------------------------------------------------------------------------------
// Helpers
//------------------------------------------------------------------------------

var SENTINEL_TYPE = /^(?:[a-zA-Z]+?Statement|ArrowFunctionExpression|FunctionExpression|ClassExpression)$/;

/**
 * Checks whether or not a node is enclosed in parentheses.
 * @param {Node|null} node - A node to check.
 * @param {sourceCode} sourceCode - The ESLint SourceCode object.
 * @returns {boolean} Whether or not the node is enclosed in parentheses.
 */
function isEnclosedInParens(node, sourceCode) {
    var prevToken = sourceCode.getTokenBefore(node);
    var nextToken = sourceCode.getTokenAfter(node);

    return prevToken && prevToken.value === "(" && nextToken && nextToken.value === ")";
}

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = {
    meta: {
        docs: {
            description: "disallow assignment operators in `return` statements",
            category: "Best Practices",
            recommended: false
        },

        schema: [
            {
                enum: ["except-parens", "always"]
            }
        ]
    },

    create: function(context) {
        var always = (context.options[0] || "except-parens") !== "except-parens";
        var sourceCode = context.getSourceCode();

        return {
            AssignmentExpression: function(node) {
                if (!always && isEnclosedInParens(node, sourceCode)) {
                    return;
                }

                var parent = node.parent;

                // Find ReturnStatement or ArrowFunctionExpression in ancestors.
                while (parent && !SENTINEL_TYPE.test(parent.type)) {
                    node = parent;
                    parent = parent.parent;
                }

                // Reports.
                if (parent && parent.type === "ReturnStatement") {
                    context.report({
                        node: parent,
                        message: "Return statement should not contain assignment."
                    });
                } else if (parent && parent.type === "ArrowFunctionExpression" && parent.body === node) {
                    context.report({
                        node: parent,
                        message: "Arrow function should not return assignment."
                    });
                }
            }
        };
    }
};
