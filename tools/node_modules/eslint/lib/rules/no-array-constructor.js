/**
 * @fileoverview Disallow construction of dense arrays using the Array constructor
 * @author Matt DuVall <http://www.mattduvall.com/>
 */

"use strict";

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

/** @type {import('../shared/types').Rule} */
module.exports = {
    meta: {
        type: "suggestion",

        docs: {
            description: "disallow `Array` constructors",
            recommended: false,
            url: "https://eslint.org/docs/rules/no-array-constructor"
        },

        schema: [],

        messages: {
            preferLiteral: "The array literal notation [] is preferable."
        }
    },

    create(context) {

        /**
         * Disallow construction of dense arrays using the Array constructor
         * @param {ASTNode} node node to evaluate
         * @returns {void}
         * @private
         */
        function check(node) {
            if (
                node.arguments.length !== 1 &&
                node.callee.type === "Identifier" &&
                node.callee.name === "Array"
            ) {
                context.report({ node, messageId: "preferLiteral" });
            }
        }

        return {
            CallExpression: check,
            NewExpression: check
        };

    }
};
