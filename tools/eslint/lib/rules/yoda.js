/**
 * @fileoverview Rule to require or disallow yoda comparisons
 * @author Nicholas C. Zakas
 * @copyright 2014 Nicholas C. Zakas. All rights reserved.
 * @copyright 2014 Brandon Mills. All rights reserved.
 */
"use strict";

//--------------------------------------------------------------------------
// Helpers
//--------------------------------------------------------------------------

/**
 * Determines whether an operator is a comparison operator.
 * @param {String} operator The operator to check.
 * @returns {boolean} Whether or not it is a comparison operator.
 */
function isComparisonOperator(operator) {
    return (/^(==|===|!=|!==|<|>|<=|>=)$/).test(operator);
}

/**
 * Determines whether an operator is an equality operator.
 * @param {String} operator The operator to check.
 * @returns {boolean} Whether or not it is an equality operator.
 */
function isEqualityOperator(operator) {
    return (/^(==|===)$/).test(operator);
}

/**
 * Determines whether an operator is one used in a range test.
 * Allowed operators are `<` and `<=`.
 * @param {String} operator The operator to check.
 * @returns {boolean} Whether the operator is used in range tests.
 */
function isRangeTestOperator(operator) {
    return ["<", "<="].indexOf(operator) >= 0;
}

/**
 * Determines whether a non-Literal node is a negative number that should be
 * treated as if it were a single Literal node.
 * @param {ASTNode} node Node to test.
 * @returns {boolean} True if the node is a negative number that looks like a
 *                    real literal and should be treated as such.
 */
function looksLikeLiteral(node) {
    return (node.type === "UnaryExpression" &&
        node.operator === "-" &&
        node.prefix &&
        node.argument.type === "Literal" &&
        typeof node.argument.value === "number");
}

/**
 * Attempts to derive a Literal node from nodes that are treated like literals.
 * @param {ASTNode} node Node to normalize.
 * @returns {ASTNode} The original node if the node is already a Literal, or a
 *                    normalized Literal node with the negative number as the
 *                    value if the node represents a negative number literal,
 *                    otherwise null if the node cannot be converted to a
 *                    normalized literal.
 */
function getNormalizedLiteral(node) {
    if (node.type === "Literal") {
        return node;
    }

    if (looksLikeLiteral(node)) {
        return {
            type: "Literal",
            value: -node.argument.value,
            raw: "-" + node.argument.value
        };
    }

    return null;
}

/**
 * Checks whether two expressions reference the same value. For example:
 *     a = a
 *     a.b = a.b
 *     a[0] = a[0]
 *     a['b'] = a['b']
 * @param   {ASTNode} a Left side of the comparison.
 * @param   {ASTNode} b Right side of the comparison.
 * @returns {boolean}   True if both sides match and reference the same value.
 */
function same(a, b) {
    if (a.type !== b.type) {
        return false;
    }

    switch (a.type) {
        case "Identifier":
            return a.name === b.name;

        case "Literal":
            return a.value === b.value;

        case "MemberExpression":

            // x[0] = x[0]
            // x[y] = x[y]
            // x.y = x.y
            return same(a.object, b.object) && same(a.property, b.property);

        case "ThisExpression":
            return true;

        default:
            return false;
    }
}

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = function(context) {

    // Default to "never" (!always) if no option
    var always = (context.options[0] === "always");
    var exceptRange = (context.options[1] && context.options[1].exceptRange);
    var onlyEquality = (context.options[1] && context.options[1].onlyEquality);

    /**
     * Determines whether node represents a range test.
     * A range test is a "between" test like `(0 <= x && x < 1)` or an "outside"
     * test like `(x < 0 || 1 <= x)`. It must be wrapped in parentheses, and
     * both operators must be `<` or `<=`. Finally, the literal on the left side
     * must be less than or equal to the literal on the right side so that the
     * test makes any sense.
     * @param {ASTNode} node LogicalExpression node to test.
     * @returns {Boolean} Whether node is a range test.
     */
    function isRangeTest(node) {
        var left = node.left,
            right = node.right;

        /**
         * Determines whether node is of the form `0 <= x && x < 1`.
         * @returns {Boolean} Whether node is a "between" range test.
         */
        function isBetweenTest() {
            var leftLiteral, rightLiteral;

            return (node.operator === "&&" &&
                (leftLiteral = getNormalizedLiteral(left.left)) &&
                (rightLiteral = getNormalizedLiteral(right.right)) &&
                leftLiteral.value <= rightLiteral.value &&
                same(left.right, right.left));
        }

        /**
         * Determines whether node is of the form `x < 0 || 1 <= x`.
         * @returns {Boolean} Whether node is an "outside" range test.
         */
        function isOutsideTest() {
            var leftLiteral, rightLiteral;

            return (node.operator === "||" &&
                (leftLiteral = getNormalizedLiteral(left.right)) &&
                (rightLiteral = getNormalizedLiteral(right.left)) &&
                leftLiteral.value <= rightLiteral.value &&
                same(left.left, right.right));
        }

        /**
         * Determines whether node is wrapped in parentheses.
         * @returns {Boolean} Whether node is preceded immediately by an open
         *                    paren token and followed immediately by a close
         *                    paren token.
         */
        function isParenWrapped() {
            var tokenBefore, tokenAfter;

            return ((tokenBefore = context.getTokenBefore(node)) &&
                tokenBefore.value === "(" &&
                (tokenAfter = context.getTokenAfter(node)) &&
                tokenAfter.value === ")");
        }

        return (node.type === "LogicalExpression" &&
            left.type === "BinaryExpression" &&
            right.type === "BinaryExpression" &&
            isRangeTestOperator(left.operator) &&
            isRangeTestOperator(right.operator) &&
            (isBetweenTest() || isOutsideTest()) &&
            isParenWrapped());
    }

    //--------------------------------------------------------------------------
    // Public
    //--------------------------------------------------------------------------

    return {
        "BinaryExpression": always ? function(node) {

            // Comparisons must always be yoda-style: if ("blue" === color)
            if (
                (node.right.type === "Literal" || looksLikeLiteral(node.right)) &&
                !(node.left.type === "Literal" || looksLikeLiteral(node.left)) &&
                !(!isEqualityOperator(node.operator) && onlyEquality) &&
                isComparisonOperator(node.operator) &&
                !(exceptRange && isRangeTest(context.getAncestors().pop()))
            ) {
                context.report(node, "Expected literal to be on the left side of " + node.operator + ".");
            }

        } : function(node) {

            // Comparisons must never be yoda-style (default)
            if (
                (node.left.type === "Literal" || looksLikeLiteral(node.left)) &&
                !(node.right.type === "Literal" || looksLikeLiteral(node.right)) &&
                !(!isEqualityOperator(node.operator) && onlyEquality) &&
                isComparisonOperator(node.operator) &&
                !(exceptRange && isRangeTest(context.getAncestors().pop()))
            ) {
                context.report(node, "Expected literal to be on the right side of " + node.operator + ".");
            }

        }
    };

};

module.exports.schema = [
    {
        "enum": ["always", "never"]
    },
    {
        "type": "object",
        "properties": {
            "exceptRange": {
                "type": "boolean"
            },
            "onlyEquality": {
                "type": "boolean"
            }
        },
        "additionalProperties": false
    }
];
