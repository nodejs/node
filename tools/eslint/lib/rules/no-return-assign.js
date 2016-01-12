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

module.exports = function(context) {
    var always = (context.options[0] || "except-parens") !== "except-parens";

    return {
        "ReturnStatement": function(node) {
            if (isAssignment(node.argument) && (always || !isEnclosedInParens(node.argument, context))) {
                context.report(node, "Return statement should not contain assignment.");
            }
        }
    };
};

module.exports.schema = [
    {
        "enum": ["except-parens", "always"]
    }
];
