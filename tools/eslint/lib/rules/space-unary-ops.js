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
    * Check if the parent unary operator is "!" in order to know if it's "!!" convert to Boolean or just "!" negation
    * @param {ASTnode} node AST node
    * @returns {boolean} Whether or not the parent is unary "!" operator
    */
    function isParentUnaryBangExpression(node) {
        return node && node.parent && node.parent.type === "UnaryExpression" && node.parent.operator === "!";
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
    * @returns {void}
    */
    function checkUnaryWordOperatorForSpaces(node, firstToken, secondToken) {
        if (options.words) {
            if (secondToken.range[0] === firstToken.range[1]) {
                context.report(node, "Unary word operator \"" + firstToken.value + "\" must be followed by whitespace.");
            }
        }

        if (!options.words && isArgumentObjectExpression(node)) {
            if (secondToken.range[0] > firstToken.range[1]) {
                context.report(node, "Unexpected space after unary word operator \"" + firstToken.value + "\".");
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
            return void 0;
        }

        if (options.nonwords) {
            if (node.prefix) {
                if (isParentUnaryBangExpression(node)) {
                    return void 0;
                }
                if (firstToken.range[1] === secondToken.range[0]) {
                    context.report(node, "Unary operator \"" + firstToken.value + "\" must be followed by whitespace.");
                }
            } else {
                if (firstToken.range[1] === secondToken.range[0]) {
                    context.report(node, "Space is required before unary expressions \"" + secondToken.value + "\".");
                }
            }
        } else {
            if (node.prefix) {
                if (secondToken.range[0] > firstToken.range[1]) {
                    context.report(node, "Unexpected space after unary operator \"" + firstToken.value + "\".");
                }
            } else {
                if (secondToken.range[0] > firstToken.range[1]) {
                    context.report(node, "Unexpected space before unary operator \"" + secondToken.value + "\".");
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
        "NewExpression": checkForSpaces
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
