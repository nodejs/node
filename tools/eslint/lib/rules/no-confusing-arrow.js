/**
 * @fileoverview A rule to warn against using arrow functions when they could be
 * confused with comparisions
 * @author Jxck <https://github.com/Jxck>
 */

"use strict";

var astUtils = require("../ast-utils.js");

//------------------------------------------------------------------------------
// Helpers
//------------------------------------------------------------------------------

/**
 * Checks whether or not a node is a conditional expression.
 * @param {ASTNode} node - node to test
 * @returns {boolean} `true` if the node is a conditional expression.
 */
function isConditional(node) {
    return node && node.type === "ConditionalExpression";
}

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = {
    meta: {
        docs: {
            description: "disallow arrow functions where they could be confused with comparisons",
            category: "ECMAScript 6",
            recommended: false
        },

        schema: [{
            type: "object",
            properties: {
                allowParens: {type: "boolean"}
            },
            additionalProperties: false
        }]
    },

    create: function(context) {
        var config = context.options[0] || {};
        var sourceCode = context.getSourceCode();

        /**
         * Reports if an arrow function contains an ambiguous conditional.
         * @param {ASTNode} node - A node to check and report.
         * @returns {void}
         */
        function checkArrowFunc(node) {
            var body = node.body;

            if (isConditional(body) && !(config.allowParens && astUtils.isParenthesised(sourceCode, body))) {
                context.report(node, "Arrow function used ambiguously with a conditional expression.");
            }
        }

        return {
            ArrowFunctionExpression: checkArrowFunc
        };
    }
};
