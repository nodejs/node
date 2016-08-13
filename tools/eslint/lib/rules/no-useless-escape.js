/**
 * @fileoverview Look for useless escapes in strings and regexes
 * @author Onur Temizkan
 */

"use strict";

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

const VALID_STRING_ESCAPES = [
    "\\",
    "n",
    "r",
    "v",
    "t",
    "b",
    "f",
    "u",
    "x",
    "\n",
    "\r"
];

const VALID_REGEX_ESCAPES = [
    "\\",
    ".",
    "-",
    "^",
    "$",
    "*",
    "+",
    "?",
    "{",
    "}",
    "[",
    "]",
    "|",
    "(",
    ")",
    "b",
    "B",
    "c",
    "d",
    "D",
    "f",
    "n",
    "r",
    "s",
    "S",
    "t",
    "v",
    "w",
    "W",
    "x",
    "u"
];

module.exports = {
    meta: {
        docs: {
            description: "disallow unnecessary escape characters",
            category: "Best Practices",
            recommended: false
        },

        schema: []
    },

    create: function(context) {

        /**
         * Checks if the escape character in given slice is unnecessary.
         *
         * @private
         * @param {string[]} escapes - list of valid escapes
         * @param {ASTNode} node - node to validate.
         * @param {string} elm - string slice to validate.
         * @returns {void}
         */
        function validate(escapes, node, elm) {
            const escapeNotFound = escapes.indexOf(elm[0][1]) === -1;
            const isQuoteEscape = elm[0][1] === node.raw[0];

            if (escapeNotFound && !isQuoteEscape) {
                context.report({
                    node: node,
                    loc: {
                        line: node.loc.start.line,
                        column: node.loc.start.column + elm.index
                    },
                    message: "Unnecessary escape character: " + elm[0] + "."
                });
            }
        }

        /**
         * Checks if a node has an escape.
         *
         * @param {ASTNode} node - node to check.
         * @returns {void}
         */
        function check(node) {
            let nodeEscapes, match;
            const pattern = /\\[^\d]/g;

            if (typeof node.value === "string") {

                // JSXAttribute doesn't have any escape sequence: https://facebook.github.io/jsx/
                if (node.parent.type === "JSXAttribute") {
                    return;
                }

                nodeEscapes = VALID_STRING_ESCAPES;
            } else if (node.regex) {
                nodeEscapes = VALID_REGEX_ESCAPES;
            } else {
                return;
            }

            while ((match = pattern.exec(node.raw))) {
                validate(nodeEscapes, node, match);
            }
        }
        return {
            Literal: check
        };
    }
};
