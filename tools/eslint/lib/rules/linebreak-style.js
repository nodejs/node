/**
 * @fileoverview Rule to enforce a single linebreak style.
 * @author Erik Mueller
 * @copyright 2015 Varun Verma. All rights reserverd.
 * @copyright 2015 James Whitney. All rights reserved.
 * @copyright 2015 Erik Mueller. All rights reserved.
 */

"use strict";

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = function(context) {

    var EXPECTED_LF_MSG = "Expected linebreaks to be 'LF' but found 'CRLF'.",
        EXPECTED_CRLF_MSG = "Expected linebreaks to be 'CRLF' but found 'LF'.";

    //--------------------------------------------------------------------------
    // Helpers
    //--------------------------------------------------------------------------

    /**
     * Builds a fix function that replaces text at the specified range in the source text.
     * @param {int[]} range The range to replace
     * @param {string} text The text to insert.
     * @returns {function} Fixer function
     * @private
     */
    function createFix(range, text) {
        return function(fixer) {
            return fixer.replaceTextRange(range, text);
        };
    }

    //--------------------------------------------------------------------------
    // Public
    //--------------------------------------------------------------------------

    return {
        "Program": function checkForlinebreakStyle(node) {
            var linebreakStyle = context.options[0] || "unix",
                expectedLF = linebreakStyle === "unix",
                expectedLFChars = expectedLF ? "\n" : "\r\n",
                source = context.getSource(),
                pattern = /\r\n|\r|\n|\u2028|\u2029/g,
                match,
                index,
                range;

            var i = 0;

            while ((match = pattern.exec(source)) !== null) {
                i++;
                if (match[0] === expectedLFChars) {
                    continue;
                }

                index = match.index;
                range = [index, index + match[0].length];
                context.report({
                    node: node,
                    loc: {
                        line: i,
                        column: context.getSourceLines()[i - 1].length
                    },
                    message: expectedLF ? EXPECTED_LF_MSG : EXPECTED_CRLF_MSG,
                    fix: createFix(range, expectedLFChars)
                });
            }
        }
    };
};

module.exports.schema = [
    {
        "enum": ["unix", "windows"]
    }
];
