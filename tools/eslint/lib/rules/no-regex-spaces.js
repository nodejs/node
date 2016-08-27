/**
 * @fileoverview Rule to count multiple spaces in regular expressions
 * @author Matt DuVall <http://www.mattduvall.com/>
 */

"use strict";

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = {
    meta: {
        docs: {
            description: "disallow multiple spaces in regular expressions",
            category: "Possible Errors",
            recommended: true
        },

        schema: []
    },

    create(context) {
        const sourceCode = context.getSourceCode();

        /**
         * Validate regular expressions
         * @param {ASTNode} node node to validate
         * @param {string} value regular expression to validate
         * @returns {void}
         * @private
         */
        function checkRegex(node, value) {
            const multipleSpacesRegex = /( {2,})+?/,
                regexResults = multipleSpacesRegex.exec(value);

            if (regexResults !== null) {
                context.report(node, "Spaces are hard to count. Use {" + regexResults[0].length + "}.");
            }
        }

        /**
         * Validate regular expression literals
         * @param {ASTNode} node node to validate
         * @returns {void}
         * @private
         */
        function checkLiteral(node) {
            const token = sourceCode.getFirstToken(node),
                nodeType = token.type,
                nodeValue = token.value;

            if (nodeType === "RegularExpression") {
                checkRegex(node, nodeValue);
            }
        }

        /**
         * Check if node is a string
         * @param {ASTNode} node node to evaluate
         * @returns {boolean} True if its a string
         * @private
         */
        function isString(node) {
            return node && node.type === "Literal" && typeof node.value === "string";
        }

        /**
         * Validate strings passed to the RegExp constructor
         * @param {ASTNode} node node to validate
         * @returns {void}
         * @private
         */
        function checkFunction(node) {
            if (node.callee.type === "Identifier" && node.callee.name === "RegExp" && isString(node.arguments[0])) {
                checkRegex(node, node.arguments[0].value);
            }
        }

        return {
            Literal: checkLiteral,
            CallExpression: checkFunction,
            NewExpression: checkFunction
        };

    }
};
