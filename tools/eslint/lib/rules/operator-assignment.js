/**
 * @fileoverview Rule to replace assignment expressions with operator assignment
 * @author Brandon Mills
 */
"use strict";

//------------------------------------------------------------------------------
// Helpers
//------------------------------------------------------------------------------

/**
 * Checks whether an operator is commutative and has an operator assignment
 * shorthand form.
 * @param   {string}  operator Operator to check.
 * @returns {boolean}          True if the operator is commutative and has a
 *     shorthand form.
 */
function isCommutativeOperatorWithShorthand(operator) {
    return ["*", "&", "^", "|"].indexOf(operator) >= 0;
}

/**
 * Checks whether an operator is not commuatative and has an operator assignment
 * shorthand form.
 * @param   {string}  operator Operator to check.
 * @returns {boolean}          True if the operator is not commuatative and has
 *     a shorthand form.
 */
function isNonCommutativeOperatorWithShorthand(operator) {
    return ["+", "-", "/", "%", "<<", ">>", ">>>"].indexOf(operator) >= 0;
}

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

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

            /*
             * x[0] = x[0]
             * x[y] = x[y]
             * x.y = x.y
             */
            return same(a.object, b.object) && same(a.property, b.property);

        default:
            return false;
    }
}

module.exports = {
    meta: {
        docs: {
            description: "require or disallow assignment operator shorthand where possible",
            category: "Stylistic Issues",
            recommended: false
        },

        schema: [
            {
                enum: ["always", "never"]
            }
        ]
    },

    create: function(context) {

        /**
         * Ensures that an assignment uses the shorthand form where possible.
         * @param   {ASTNode} node An AssignmentExpression node.
         * @returns {void}
         */
        function verify(node) {
            if (node.operator !== "=" || node.right.type !== "BinaryExpression") {
                return;
            }

            const left = node.left;
            const expr = node.right;
            const operator = expr.operator;

            if (isCommutativeOperatorWithShorthand(operator)) {
                if (same(left, expr.left) || same(left, expr.right)) {
                    context.report(node, "Assignment can be replaced with operator assignment.");
                }
            } else if (isNonCommutativeOperatorWithShorthand(operator)) {
                if (same(left, expr.left)) {
                    context.report(node, "Assignment can be replaced with operator assignment.");
                }
            }
        }

        /**
         * Warns if an assignment expression uses operator assignment shorthand.
         * @param   {ASTNode} node An AssignmentExpression node.
         * @returns {void}
         */
        function prohibit(node) {
            if (node.operator !== "=") {
                context.report(node, "Unexpected operator assignment shorthand.");
            }
        }

        return {
            AssignmentExpression: context.options[0] !== "never" ? verify : prohibit
        };

    }
};
