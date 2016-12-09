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

    create(context) {

        /**
         * Checks if the escape character in given slice is unnecessary.
         *
         * @private
         * @param {string[]} escapes - list of valid escapes
         * @param {ASTNode} node - node to validate.
         * @param {string} match - string slice to validate.
         * @returns {void}
         */
        function validate(escapes, node, match) {
            const isTemplateElement = node.type === "TemplateElement";
            const escapedChar = match[0][1];
            let isUnnecessaryEscape = escapes.indexOf(escapedChar) === -1;
            let isQuoteEscape;

            if (isTemplateElement) {
                isQuoteEscape = escapedChar === "`";

                if (escapedChar === "$") {

                    // Warn if `\$` is not followed by `{`
                    isUnnecessaryEscape = match.input[match.index + 2] !== "{";
                } else if (escapedChar === "{") {

                    /* Warn if `\{` is not preceded by `$`. If preceded by `$`, escaping
                     * is necessary and the rule should not warn. If preceded by `/$`, the rule
                     * will warn for the `/$` instead, as it is the first unnecessarily escaped character.
                     */
                    isUnnecessaryEscape = match.input[match.index - 1] !== "$";
                }
            } else {
                isQuoteEscape = escapedChar === node.raw[0];
            }

            if (isUnnecessaryEscape && !isQuoteEscape) {
                context.report({
                    node,
                    loc: {
                        line: node.loc.start.line,
                        column: node.loc.start.column + match.index
                    },
                    message: "Unnecessary escape character: {{character}}.",
                    data: {
                        character: match[0]
                    }
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
            const isTemplateElement = node.type === "TemplateElement";
            const value = isTemplateElement ? node.value.raw : node.raw;
            const pattern = /\\[^\d]/g;
            let nodeEscapes,
                match;

            if (typeof node.value === "string" || isTemplateElement) {

                /*
                 * JSXAttribute doesn't have any escape sequence: https://facebook.github.io/jsx/.
                 * In addition, backticks are not supported by JSX yet: https://github.com/facebook/jsx/issues/25.
                 */
                if (node.parent.type === "JSXAttribute") {
                    return;
                }

                nodeEscapes = VALID_STRING_ESCAPES;
            } else if (node.regex) {
                nodeEscapes = VALID_REGEX_ESCAPES;
            } else {
                return;
            }

            while ((match = pattern.exec(value))) {
                validate(nodeEscapes, node, match);
            }
        }

        return {
            Literal: check,
            TemplateElement: check
        };
    }
};
