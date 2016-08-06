/**
 * @fileoverview Rule to flag when return statement contains assignment
 * @author Ilya Volodin
 */
"use strict";

//------------------------------------------------------------------------------
// Helpers
//------------------------------------------------------------------------------

let SENTINEL_TYPE = /^(?:[a-zA-Z]+?Statement|ArrowFunctionExpression|FunctionExpression|ClassExpression)$/;

/**
 * Checks whether or not a node is enclosed in parentheses.
 * @param {Node|null} node - A node to check.
 * @param {sourceCode} sourceCode - The ESLint SourceCode object.
 * @returns {boolean} Whether or not the node is enclosed in parentheses.
 */
function isEnclosedInParens(node, sourceCode) {
    let prevToken = sourceCode.getTokenBefore(node);
    let nextToken = sourceCode.getTokenAfter(node);

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
        let always = (context.options[0] || "except-parens") !== "except-parens";
        let sourceCode = context.getSourceCode();

        return {
            AssignmentExpression: function(node) {
                if (!always && isEnclosedInParens(node, sourceCode)) {
                    return;
                }

                let parent = node.parent;

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
