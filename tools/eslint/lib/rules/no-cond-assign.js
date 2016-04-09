/**
 * @fileoverview Rule to flag assignment in a conditional statement's test expression
 * @author Stephen Murray <spmurrayzzz>
 */
"use strict";

var NODE_DESCRIPTIONS = {
    "DoWhileStatement": "a 'do...while' statement",
    "ForStatement": "a 'for' statement",
    "IfStatement": "an 'if' statement",
    "WhileStatement": "a 'while' statement"
};

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = function(context) {

    var prohibitAssign = (context.options[0] || "except-parens");

    /**
     * Check whether an AST node is the test expression for a conditional statement.
     * @param {!Object} node The node to test.
     * @returns {boolean} `true` if the node is the text expression for a conditional statement; otherwise, `false`.
     */
    function isConditionalTestExpression(node) {
        return node.parent &&
            node.parent.test &&
            node === node.parent.test;
    }

    /**
     * Given an AST node, perform a bottom-up search for the first ancestor that represents a conditional statement.
     * @param {!Object} node The node to use at the start of the search.
     * @returns {?Object} The closest ancestor node that represents a conditional statement.
     */
    function findConditionalAncestor(node) {
        var currentAncestor = node;

        do {
            if (isConditionalTestExpression(currentAncestor)) {
                return currentAncestor.parent;
            }
        } while ((currentAncestor = currentAncestor.parent));

        return null;
    }

    /**
     * Check whether the code represented by an AST node is enclosed in parentheses.
     * @param {!Object} node The node to test.
     * @returns {boolean} `true` if the code is enclosed in parentheses; otherwise, `false`.
     */
    function isParenthesised(node) {
        var previousToken = context.getTokenBefore(node),
            nextToken = context.getTokenAfter(node);

        return previousToken.value === "(" && previousToken.range[1] <= node.range[0] &&
            nextToken.value === ")" && nextToken.range[0] >= node.range[1];
    }

    /**
     * Check whether the code represented by an AST node is enclosed in two sets of parentheses.
     * @param {!Object} node The node to test.
     * @returns {boolean} `true` if the code is enclosed in two sets of parentheses; otherwise, `false`.
     */
    function isParenthesisedTwice(node) {
        var previousToken = context.getTokenBefore(node, 1),
            nextToken = context.getTokenAfter(node, 1);

        return isParenthesised(node) &&
            previousToken.value === "(" && previousToken.range[1] <= node.range[0] &&
            nextToken.value === ")" && nextToken.range[0] >= node.range[1];
    }

    /**
     * Check a conditional statement's test expression for top-level assignments that are not enclosed in parentheses.
     * @param {!Object} node The node for the conditional statement.
     * @returns {void}
     */
    function testForAssign(node) {
        if (node.test &&
            (node.test.type === "AssignmentExpression") &&
            (node.type === "ForStatement" ?
                !isParenthesised(node.test) :
                !isParenthesisedTwice(node.test)
            )
        ) {

            // must match JSHint's error message
            context.report({
                node: node,
                loc: node.test.loc.start,
                message: "Expected a conditional expression and instead saw an assignment."
            });
        }
    }

    /**
     * Check whether an assignment expression is descended from a conditional statement's test expression.
     * @param {!Object} node The node for the assignment expression.
     * @returns {void}
     */
    function testForConditionalAncestor(node) {
        var ancestor = findConditionalAncestor(node);

        if (ancestor) {
            context.report(ancestor, "Unexpected assignment within {{type}}.", {
                type: NODE_DESCRIPTIONS[ancestor.type] || ancestor.type
            });
        }
    }

    if (prohibitAssign === "always") {
        return {
            "AssignmentExpression": testForConditionalAncestor
        };
    }

    return {
        "DoWhileStatement": testForAssign,
        "ForStatement": testForAssign,
        "IfStatement": testForAssign,
        "WhileStatement": testForAssign
    };

};

module.exports.schema = [
    {
        "enum": ["except-parens", "always"]
    }
];
