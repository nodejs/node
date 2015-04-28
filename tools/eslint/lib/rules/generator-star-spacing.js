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

    var mode = {
        before: { before: true, after: false },
        after: { before: false, after: true },
        both: { before: true, after: true },
        neither: { before: false, after: false }
    }[context.options[0] || "before"];

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
            context.report(
                leftToken.value === "*" ? leftToken : rightToken,
                "{{type}} space {{side}} *.",
                {
                    type: mode[side] ? "Missing" : "Unexpected",
                    side: side
                }
            );
        }
    }

    /**
     * Enforces the spacing around the star if node is a generator function.
     * @param {ASTNode} node A function expression or declaration node.
     * @returns {void}
     */
    function checkFunction(node) {
        var isMethod, starToken, tokenBefore, tokenAfter;

        if (!node.generator) {
            return;
        }

        isMethod = !!context.getAncestors().pop().method;

        if (isMethod) {
            starToken = context.getTokenBefore(node, 1);
        } else {
            starToken = context.getFirstToken(node, 1);
        }

        // Only check before when preceded by `function` keyword
        tokenBefore = context.getTokenBefore(starToken);
        if (tokenBefore.value === "function") {
            checkSpacing("before", tokenBefore, starToken);
        }

        // Only check after when followed by an identifier
        tokenAfter = context.getTokenAfter(starToken);
        if (tokenAfter.type === "Identifier") {
            checkSpacing("after", starToken, tokenAfter);
        }
    }

    return {
        "FunctionDeclaration": checkFunction,
        "FunctionExpression": checkFunction
    };

};
