/**
 * @fileoverview Rule to check the spacing around the * in yield* expressions.
 * @author Bryan Smith
 * @copyright 2015 Bryan Smith. All rights reserved.
 */

"use strict";

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = function(context) {
    var sourceCode = context.getSourceCode();

    var mode = (function(option) {
        if (!option || typeof option === "string") {
            return {
                before: { before: true, after: false },
                after: { before: false, after: true },
                both: { before: true, after: true },
                neither: { before: false, after: false }
            }[option || "after"];
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
        if (sourceCode.isSpaceBetweenTokens(leftToken, rightToken) !== mode[side]) {
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
     * Enforces the spacing around the star if node is a yield* expression.
     * @param {ASTNode} node A yield expression node.
     * @returns {void}
     */
    function checkExpression(node) {
        if (!node.delegate) {
            return;
        }

        var tokens = sourceCode.getFirstTokens(node, 3);
        var yieldToken = tokens[0];
        var starToken = tokens[1];
        var nextToken = tokens[2];

        checkSpacing("before", yieldToken, starToken);
        checkSpacing("after", starToken, nextToken);
    }

    return {
        "YieldExpression": checkExpression
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
