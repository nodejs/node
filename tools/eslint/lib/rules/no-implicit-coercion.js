/**
 * @fileoverview A rule to disallow the type conversions with shorter notations.
 * @author Toru Nagashima
 * @copyright 2015 Toru Nagashima. All rights reserved.
 */

"use strict";

//------------------------------------------------------------------------------
// Helpers
//------------------------------------------------------------------------------

var INDEX_OF_PATTERN = /^(?:i|lastI)ndexOf$/;
var ALLOWABLE_OPERATORS = ["~", "!!", "+", "*"];

/**
 * Parses and normalizes an option object.
 * @param {object} options - An option object to parse.
 * @returns {object} The parsed and normalized option object.
 */
function parseOptions(options) {
    options = options || {};
    return {
        boolean: "boolean" in options ? Boolean(options.boolean) : true,
        number: "number" in options ? Boolean(options.number) : true,
        string: "string" in options ? Boolean(options.string) : true,
        allow: options.allow || []
    };
}

/**
 * Checks whether or not a node is a double logical nigating.
 * @param {ASTNode} node - An UnaryExpression node to check.
 * @returns {boolean} Whether or not the node is a double logical nigating.
 */
function isDoubleLogicalNegating(node) {
    return (
        node.operator === "!" &&
        node.argument.type === "UnaryExpression" &&
        node.argument.operator === "!"
    );
}

/**
 * Checks whether or not a node is a binary negating of `.indexOf()` method calling.
 * @param {ASTNode} node - An UnaryExpression node to check.
 * @returns {boolean} Whether or not the node is a binary negating of `.indexOf()` method calling.
 */
function isBinaryNegatingOfIndexOf(node) {
    return (
        node.operator === "~" &&
        node.argument.type === "CallExpression" &&
        node.argument.callee.type === "MemberExpression" &&
        node.argument.callee.property.type === "Identifier" &&
        INDEX_OF_PATTERN.test(node.argument.callee.property.name)
    );
}

/**
 * Checks whether or not a node is a multiplying by one.
 * @param {BinaryExpression} node - A BinaryExpression node to check.
 * @returns {boolean} Whether or not the node is a multiplying by one.
 */
function isMultiplyByOne(node) {
    return node.operator === "*" && (
        node.left.type === "Literal" && node.left.value === 1 ||
        node.right.type === "Literal" && node.right.value === 1
    );
}

/**
 * Checks whether the result of a node is numeric or not
 * @param {ASTNode} node The node to test
 * @returns {boolean} true if the node is a number literal or a `Number()`, `parseInt` or `parseFloat` call
 */
function isNumeric(node) {
    return (
        node.type === "Literal" && typeof node.value === "number" ||
        node.type === "CallExpression" && (
            node.callee.name === "Number" ||
            node.callee.name === "parseInt" ||
            node.callee.name === "parseFloat"
        )
    );
}

/**
 * Returns the first non-numeric operand in a BinaryExpression. Designed to be
 * used from bottom to up since it walks up the BinaryExpression trees using
 * node.parent to find the result.
 * @param {BinaryExpression} node The BinaryExpression node to be walked up on
 * @returns {ASTNode|null} The first non-numeric item in the BinaryExpression tree or null
 */
function getNonNumericOperand(node) {
    var left = node.left,
        right = node.right;

    if (right.type !== "BinaryExpression" && !isNumeric(right)) {
        return right;
    }

    if (left.type !== "BinaryExpression" && !isNumeric(left)) {
        return left;
    }

    return null;
}

/**
 * Checks whether or not a node is a concatenating with an empty string.
 * @param {ASTNode} node - A BinaryExpression node to check.
 * @returns {boolean} Whether or not the node is a concatenating with an empty string.
 */
function isConcatWithEmptyString(node) {
    return node.operator === "+" && (
        (node.left.type === "Literal" && node.left.value === "") ||
        (node.right.type === "Literal" && node.right.value === "")
    );
}

/**
 * Checks whether or not a node is appended with an empty string.
 * @param {ASTNode} node - An AssignmentExpression node to check.
 * @returns {boolean} Whether or not the node is appended with an empty string.
 */
function isAppendEmptyString(node) {
    return node.operator === "+=" && node.right.type === "Literal" && node.right.value === "";
}

/**
 * Gets a node that is the left or right operand of a node, is not the specified literal.
 * @param {ASTNode} node - A BinaryExpression node to get.
 * @param {any} value - A literal value to check.
 * @returns {ASTNode} A node that is the left or right operand of the node, is not the specified literal.
 */
function getOtherOperand(node, value) {
    if (node.left.type === "Literal" && node.left.value === value) {
        return node.right;
    }
    return node.left;
}

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = function(context) {
    var options = parseOptions(context.options[0]),
        operatorAllowed = false;

    return {
        "UnaryExpression": function(node) {

            // !!foo
            operatorAllowed = options.allow.indexOf("!!") >= 0;
            if (!operatorAllowed && options.boolean && isDoubleLogicalNegating(node)) {
                context.report(
                    node,
                    "use `Boolean({{code}})` instead.", {
                        code: context.getSource(node.argument.argument)
                    });
            }

            // ~foo.indexOf(bar)
            operatorAllowed = options.allow.indexOf("~") >= 0;
            if (!operatorAllowed && options.boolean && isBinaryNegatingOfIndexOf(node)) {
                context.report(
                    node,
                    "use `{{code}} !== -1` instead.", {
                        code: context.getSource(node.argument)
                    });
            }

            // +foo
            operatorAllowed = options.allow.indexOf("+") >= 0;
            if (!operatorAllowed && options.number && node.operator === "+" && !isNumeric(node.argument)) {
                context.report(
                    node,
                    "use `Number({{code}})` instead.", {
                        code: context.getSource(node.argument)
                    });
            }
        },

        // Use `:exit` to prevent double reporting
        "BinaryExpression:exit": function(node) {

            // 1 * foo
            operatorAllowed = options.allow.indexOf("*") >= 0;
            var nonNumericOperand = !operatorAllowed && options.number && isMultiplyByOne(node) && getNonNumericOperand(node);

            if (nonNumericOperand) {
                context.report(
                    node,
                    "use `Number({{code}})` instead.", {
                        code: context.getSource(nonNumericOperand)
                    });
            }

            // "" + foo
            operatorAllowed = options.allow.indexOf("+") >= 0;
            if (!operatorAllowed && options.string && isConcatWithEmptyString(node)) {
                context.report(
                    node,
                    "use `String({{code}})` instead.", {
                        code: context.getSource(getOtherOperand(node, ""))
                    });
            }
        },

        "AssignmentExpression": function(node) {

            // foo += ""
            operatorAllowed = options.allow.indexOf("+") >= 0;
            if (options.string && isAppendEmptyString(node)) {
                context.report(
                    node,
                    "use `{{code}} = String({{code}})` instead.", {
                        code: context.getSource(getOtherOperand(node, ""))
                    });
            }
        }
    };
};

module.exports.schema = [{
    "type": "object",
    "properties": {
        "boolean": {
            "type": "boolean"
        },
        "number": {
            "type": "boolean"
        },
        "string": {
            "type": "boolean"
        },
        "allow": {
            "type": "array",
            "items": {
                "enum": ALLOWABLE_OPERATORS
            },
            "uniqueItems": true
        }
    },
    "additionalProperties": false
}];
