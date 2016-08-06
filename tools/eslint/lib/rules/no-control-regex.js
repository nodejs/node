/**
 * @fileoverview Rule to forbid control charactes from regular expressions.
 * @author Nicholas C. Zakas
 */

"use strict";

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = {
    meta: {
        docs: {
            description: "disallow control characters in regular expressions",
            category: "Possible Errors",
            recommended: true
        },

        schema: []
    },

    create: function(context) {

        /**
         * Get the regex expression
         * @param {ASTNode} node node to evaluate
         * @returns {*} Regex if found else null
         * @private
         */
        function getRegExp(node) {
            if (node.value instanceof RegExp) {
                return node.value;
            } else if (typeof node.value === "string") {

                let parent = context.getAncestors().pop();

                if ((parent.type === "NewExpression" || parent.type === "CallExpression") &&
                    parent.callee.type === "Identifier" && parent.callee.name === "RegExp"
                ) {

                    // there could be an invalid regular expression string
                    try {
                        return new RegExp(node.value);
                    } catch (ex) {
                        return null;
                    }
                }
            }

            return null;
        }

        /**
         * Check if given regex string has control characters in it
         * @param {string} regexStr regex as string to check
         * @returns {boolean} returns true if finds control characters on given string
         * @private
         */
        function hasControlCharacters(regexStr) {

            // check control characters, if RegExp object used
            let hasControlChars = /[\x00-\x1f]/.test(regexStr); // eslint-disable-line no-control-regex

            // check substr, if regex literal used
            let subStrIndex = regexStr.search(/\\x[01][0-9a-f]/i);

            if (!hasControlChars && subStrIndex > -1) {

                // is it escaped, check backslash count
                let possibleEscapeCharacters = regexStr.substr(0, subStrIndex).match(/\\+$/gi);

                hasControlChars = possibleEscapeCharacters === null || !(possibleEscapeCharacters[0].length % 2);
            }

            return hasControlChars;
        }

        return {
            Literal: function(node) {
                let computedValue,
                    regex = getRegExp(node);

                if (regex) {
                    computedValue = regex.toString();

                    if (hasControlCharacters(computedValue)) {
                        context.report(node, "Unexpected control character in regular expression.");
                    }
                }
            }
        };

    }
};
