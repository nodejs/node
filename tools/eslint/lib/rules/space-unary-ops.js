/**
 * @fileoverview This rule shoud require or disallow spaces before or after unary operations.
 * @author Marcin Kumorek
 * @copyright 2014 Marcin Kumorek. All rights reserved.
 */
"use strict";

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = function(context) {
    var options = context.options && Array.isArray(context.options) && context.options[0] || { words: true, nonwords: false };

    //--------------------------------------------------------------------------
    // Helpers
    //--------------------------------------------------------------------------

    /**
    * Check if the node is the first "!" in a "!!" convert to Boolean expression
    * @param {ASTnode} node AST node
    * @returns {boolean} Whether or not the node is first "!" in "!!"
    */
    function isFirstBangInBangBangExpression(node) {
        return node && node.type === "UnaryExpression" && node.argument.operator === "!" &&
            node.argument && node.argument.type === "UnaryExpression" && node.argument.operator === "!";
    }

    /**
    * Check if the node's child argument is an "ObjectExpression"
    * @param {ASTnode} node AST node
    * @returns {boolean} Whether or not the argument's type is "ObjectExpression"
    */
    function isArgumentObjectExpression(node) {
        return node.argument && node.argument.type && node.argument.type === "ObjectExpression";
    }

    /**
    * Check Unary Word Operators for spaces after the word operator
    * @param {ASTnode} node AST node
    * @param {object} firstToken first token from the AST node
    * @param {object} secondToken second token from the AST node
    * @param {string} word The word to be used for reporting
    * @returns {void}
    */
    function checkUnaryWordOperatorForSpaces(node, firstToken, secondToken, word) {
        word = word || firstToken.value;

        if (options.words) {
            if (secondToken.range[0] === firstToken.range[1]) {
                context.report({
                    node: node,
                    message: "Unary word operator '" + word + "' must be followed by whitespace.",
                    fix: function(fixer) {
                        return fixer.insertTextAfter(firstToken, " ");
                    }
                });
            }
        }

        if (!options.words && isArgumentObjectExpression(node)) {
            if (secondToken.range[0] > firstToken.range[1]) {
                context.report({
                    node: node,
                    message: "Unexpected space after unary word operator '" + word + "'.",
                    fix: function(fixer) {
                        return fixer.removeRange([firstToken.range[1], secondToken.range[0]]);
                    }
                });
            }
        }
    }

    /**
    * Checks UnaryExpression, UpdateExpression and NewExpression for spaces before and after the operator
    * @param {ASTnode} node AST node
    * @returns {void}
    */
    function checkForSpaces(node) {
        var tokens = context.getFirstTokens(node, 2),
            firstToken = tokens[0],
            secondToken = tokens[1];

        if ((node.type === "NewExpression" || node.prefix) && firstToken.type === "Keyword") {
            checkUnaryWordOperatorForSpaces(node, firstToken, secondToken);
            return;
        }

        if (options.nonwords) {
            if (node.prefix) {
                if (isFirstBangInBangBangExpression(node)) {
                    return;
                }
                if (firstToken.range[1] === secondToken.range[0]) {
                    context.report({
                        node: node,
                        message: "Unary operator '" + firstToken.value + "' must be followed by whitespace.",
                        fix: function(fixer) {
                            return fixer.insertTextAfter(firstToken, " ");
                        }
                    });
                }
            } else {
                if (firstToken.range[1] === secondToken.range[0]) {
                    context.report({
                        node: node,
                        message: "Space is required before unary expressions '" + secondToken.value + "'.",
                        fix: function(fixer) {
                            return fixer.insertTextBefore(secondToken, " ");
                        }
                    });
                }
            }
        } else {
            if (node.prefix) {
                if (secondToken.range[0] > firstToken.range[1]) {
                    context.report({
                        node: node,
                        message: "Unexpected space after unary operator '" + firstToken.value + "'.",
                        fix: function(fixer) {
                            return fixer.removeRange([firstToken.range[1], secondToken.range[0]]);
                        }
                    });
                }
            } else {
                if (secondToken.range[0] > firstToken.range[1]) {
                    context.report({
                        node: node,
                        message: "Unexpected space before unary operator '" + secondToken.value + "'.",
                        fix: function(fixer) {
                            return fixer.removeRange([firstToken.range[1], secondToken.range[0]]);
                        }
                    });
                }
            }
        }
    }

    //--------------------------------------------------------------------------
    // Public
    //--------------------------------------------------------------------------

    return {
        "UnaryExpression": checkForSpaces,
        "UpdateExpression": checkForSpaces,
        "NewExpression": checkForSpaces,
        "YieldExpression": function(node) {
            var tokens = context.getFirstTokens(node, 3),
                word = "yield";

            if (!node.argument || node.delegate) {
                return;
            }

            checkUnaryWordOperatorForSpaces(node, tokens[0], tokens[1], word);
        }
    };

};

module.exports.schema = [
    {
        "type": "object",
        "properties": {
            "words": {
                "type": "boolean"
            },
            "nonwords": {
                "type": "boolean"
            }
        },
        "additionalProperties": false
    }
];
