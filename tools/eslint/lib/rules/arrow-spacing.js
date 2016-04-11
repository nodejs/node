/**
 * @fileoverview Rule to define spacing before/after arrow function's arrow.
 * @author Jxck
 * @copyright 2015 Jxck. All rights reserved.
 */
"use strict";

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = function(context) {

    // merge rules with default
    var rule = { before: true, after: true },
        option = context.options[0] || {};

    rule.before = option.before !== false;
    rule.after = option.after !== false;

    /**
     * Get tokens of arrow(`=>`) and before/after arrow.
     * @param {ASTNode} node The arrow function node.
     * @returns {Object} Tokens of arrow and before/after arrow.
     */
    function getTokens(node) {
        var t = context.getFirstToken(node);
        var before;

        while (t.type !== "Punctuator" || t.value !== "=>") {
            before = t;
            t = context.getTokenAfter(t);
        }
        var after = context.getTokenAfter(t);

        return { before: before, arrow: t, after: after };
    }

    /**
     * Count spaces before/after arrow(`=>`) token.
     * @param {Object} tokens Tokens before/after arrow.
     * @returns {Object} count of space before/after arrow.
     */
    function countSpaces(tokens) {
        var before = tokens.arrow.range[0] - tokens.before.range[1];
        var after = tokens.after.range[0] - tokens.arrow.range[1];

        return { before: before, after: after };
    }

    /**
     * Determines whether space(s) before after arrow(`=>`) is satisfy rule.
     * if before/after value is `true`, there should be space(s).
     * if before/after value is `false`, there should be no space.
     * @param {ASTNode} node The arrow function node.
     * @returns {void}
     */
    function spaces(node) {
        var tokens = getTokens(node);
        var countSpace = countSpaces(tokens);

        if (rule.before) {

            // should be space(s) before arrow
            if (countSpace.before === 0) {
                context.report({
                    node: tokens.before,
                    message: "Missing space before =>",
                    fix: function(fixer) {
                        return fixer.insertTextBefore(tokens.arrow, " ");
                    }
                });
            }
        } else {

            // should be no space before arrow
            if (countSpace.before > 0) {
                context.report({
                    node: tokens.before,
                    message: "Unexpected space before =>",
                    fix: function(fixer) {
                        return fixer.removeRange([tokens.before.range[1], tokens.arrow.range[0]]);
                    }
                });
            }
        }

        if (rule.after) {

            // should be space(s) after arrow
            if (countSpace.after === 0) {
                context.report({
                    node: tokens.after,
                    message: "Missing space after =>",
                    fix: function(fixer) {
                        return fixer.insertTextAfter(tokens.arrow, " ");
                    }
                });
            }
        } else {

            // should be no space after arrow
            if (countSpace.after > 0) {
                context.report({
                    node: tokens.after,
                    message: "Unexpected space after =>",
                    fix: function(fixer) {
                        return fixer.removeRange([tokens.arrow.range[1], tokens.after.range[0]]);
                    }
                });
            }
        }
    }

    return {
        "ArrowFunctionExpression": spaces
    };
};

module.exports.schema = [
    {
        "type": "object",
        "properties": {
            "before": {
                "type": "boolean"
            },
            "after": {
                "type": "boolean"
            }
        },
        "additionalProperties": false
    }
];
