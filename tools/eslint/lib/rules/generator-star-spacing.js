/**
 * @fileoverview Rule to check the spacing around the * in generator functions.
 * @author Jamund Ferguson
 * @copyright 2015 Brandon Mills. All rights reserved.
 * @copyright 2014 Jamund Ferguson. All rights reserved.
 */

"use strict";

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = function(context) {

    var mode = (function(option) {
        if (!option || typeof option === "string") {
            return {
                before: { before: true, after: false },
                after: { before: false, after: true },
                both: { before: true, after: true },
                neither: { before: false, after: false }
            }[option || "before"];
        }
        return option;
    }(context.options[0]));

    /**
     * Checks the spacing between two tokens before or after the star token.
     * @param {string} side Either "before" or "after".
     * @param {Token} leftToken `function` keyword token if side is "before", or
     *     star token if side is "after".
     * @param {Token} rightToken Star token if side is "before", or identifier
     *     token if side is "after".
     * @returns {void}
     */
    function checkSpacing(side, leftToken, rightToken) {
        if (!!(rightToken.range[0] - leftToken.range[1]) !== mode[side]) {
            var after = leftToken.value === "*";
            var spaceRequired = mode[side];
            var node = after ? leftToken : rightToken;
            var type = spaceRequired ? "Missing" : "Unexpected";
            var message = type + " space " + side + " *.";

            context.report({
                node: node,
                message: message,
                fix: function(fixer) {
                    if (spaceRequired) {
                        if (after) {
                            return fixer.insertTextAfter(node, " ");
                        }
                        return fixer.insertTextBefore(node, " ");
                    }
                    return fixer.removeRange([leftToken.range[1], rightToken.range[0]]);
                }
            });
        }
    }

    /**
     * Enforces the spacing around the star if node is a generator function.
     * @param {ASTNode} node A function expression or declaration node.
     * @returns {void}
     */
    function checkFunction(node) {
        var prevToken, starToken, nextToken;

        if (!node.generator) {
            return;
        }

        if (node.parent.method || node.parent.type === "MethodDefinition") {
            starToken = context.getTokenBefore(node, 1);
        } else {
            starToken = context.getFirstToken(node, 1);
        }

        // Only check before when preceded by `function` keyword
        prevToken = context.getTokenBefore(starToken);
        if (prevToken.value === "function" || prevToken.value === "static") {
            checkSpacing("before", prevToken, starToken);
        }

        nextToken = context.getTokenAfter(starToken);
        checkSpacing("after", starToken, nextToken);
    }

    return {
        "FunctionDeclaration": checkFunction,
        "FunctionExpression": checkFunction
    };

};

module.exports.schema = [
    {
        "oneOf": [
            {
                "enum": ["before", "after", "both", "neither"]
            },
            {
                "type": "object",
                "properties": {
                    "before": {"type": "boolean"},
                    "after": {"type": "boolean"}
                },
                "additionalProperties": false
            }
        ]
    }
];
