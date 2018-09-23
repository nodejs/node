/**
 * @fileoverview Rule to flag wrapping non-iife in parens
 * @author Ilya Volodin
 * @copyright 2013 Ilya Volodin. All rights reserved.
 */

"use strict";

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = function(context) {

    /**
     * Checks a function expression to see if its surrounded by parens.
     * @param {ASTNode} node The node to check.
     * @returns {void}
     * @private
     */
    function checkFunction(node) {
        var previousToken, nextToken, isCall;

        if (node.type === "ArrowFunctionExpression" &&
                /(?:Call|New|Logical|Binary|Conditional|Update)Expression/.test(node.parent.type)
        ) {
            return;
        }

        // (function() {}).foo
        if (node.parent.type === "MemberExpression" && node.parent.object === node) {
            return;
        }

        // (function(){})()
        isCall = /CallExpression|NewExpression/.test(node.parent.type);
        if (isCall && node.parent.callee === node) {
            return;
        }

        previousToken = context.getTokenBefore(node);
        nextToken = context.getTokenAfter(node);

        // f(function(){}) and new f(function(){})
        if (isCall) {

            // if the previousToken is right after the callee
            if (node.parent.callee.range[1] === previousToken.range[0]) {
                return;
            }
        }

        if (previousToken.value === "(" && nextToken.value === ")") {
            context.report(node, "Wrapping non-IIFE function literals in parens is unnecessary.");
        }
    }

    return {
        "ArrowFunctionExpression": checkFunction,
        "FunctionExpression": checkFunction
    };

};

module.exports.schema = [];
