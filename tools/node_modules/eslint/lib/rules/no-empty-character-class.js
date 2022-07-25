/**
 * @fileoverview Rule to flag the use of empty character classes in regular expressions
 * @author Ian Christian Myers
 */

"use strict";

//------------------------------------------------------------------------------
// Helpers
//------------------------------------------------------------------------------

/*
 * plain-English description of the following regexp:
 * 0. `^` fix the match at the beginning of the string
 * 1. `([^\\[]|\\.|\[([^\\\]]|\\.)+\])*`: regexp contents; 0 or more of the following
 * 1.0. `[^\\[]`: any character that's not a `\` or a `[` (anything but escape sequences and character classes)
 * 1.1. `\\.`: an escape sequence
 * 1.2. `\[([^\\\]]|\\.)+\]`: a character class that isn't empty
 * 2. `$`: fix the match at the end of the string
 */
const regex = /^([^\\[]|\\.|\[([^\\\]]|\\.)+\])*$/u;

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

/** @type {import('../shared/types').Rule} */
module.exports = {
    meta: {
        type: "problem",

        docs: {
            description: "Disallow empty character classes in regular expressions",
            recommended: true,
            url: "https://eslint.org/docs/rules/no-empty-character-class"
        },

        schema: [],

        messages: {
            unexpected: "Empty class."
        }
    },

    create(context) {
        return {
            "Literal[regex]"(node) {
                if (!regex.test(node.regex.pattern)) {
                    context.report({ node, messageId: "unexpected" });
                }
            }
        };

    }
};
