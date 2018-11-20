/**
 * @fileoverview Rule to flag octal escape sequences in string literals.
 * @author Ian Christian Myers
 */

"use strict";

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = function(context) {

    return {

        "Literal": function(node) {
            if (typeof node.value !== "string") {
                return;
            }

            var match = node.raw.match(/^([^\\]|\\[^0-7])*\\([0-3][0-7]{1,2}|[4-7][0-7]|[0-7])/),
                octalDigit;

            if (match) {
                octalDigit = match[2];

                // \0 is actually not considered an octal
                if (match[2] !== "0" || typeof match[3] !== "undefined") {
                    context.report(node, "Don't use octal: '\\{{octalDigit}}'. Use '\\u....' instead.",
                            { octalDigit: octalDigit });
                }
            }
        }

    };

};
