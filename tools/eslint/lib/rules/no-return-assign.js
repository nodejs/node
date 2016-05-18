/**
 * @fileoverview Rule to flag when return statement contains assignment
 * @author Ilya Volodin
 */
"use strict";

//------------------------------------------------------------------------------
// Helpers
//------------------------------------------------------------------------------

/**
 * Checks whether or not a node is an `AssignmentExpression`.
 * @param {Node|null} node - A node to check.
 * @returns {boolean} Whether or not the node is an `AssignmentExpression`.
 */
function isAssignment(node) {
    return node && node.type === "AssignmentExpression";
}

/**
 * Checks whether or not a node is enclosed in parentheses.
 * @param {Node|null} node - A node to check.
 * @param {RuleContext} context - The current context.
 * @returns {boolean} Whether or not the node is enclosed in parentheses.
 */
function isEnclosedInParens(node, context) {
    var prevToken = context.getTokenBefore(node);
    var nextToken = context.getTokenAfter(node);

    return prevToken.value === "(" && nextToken.value === ")";
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

        /**
         * Check whether return statement contains assignment
         * @param {ASTNode} nodeToCheck node to check
         * @param {ASTNode} nodeToReport node to report
         * @param {string} message message to report
         * @returns {void}
         * @private
         */
        function checkForAssignInReturn(nodeToCheck, nodeToReport, message) {
            if (isAssignment(nodeToCheck) && (always || !isEnclosedInParens(nodeToCheck, context))) {
                context.report(nodeToReport, message);
            }
        }

        return {
            ReturnStatement: function(node) {
                var message = "Return statement should not contain assignment.";

                checkForAssignInReturn(node.argument, node, message);
            },
            ArrowFunctionExpression: function(node) {
                if (node.body.type !== "BlockStatement") {
                    var message = "Arrow function should not return assignment.";

                    checkForAssignInReturn(node.body, node, message);
                }
            }
        };
    }
};
