/**
 * @fileoverview Disallow trailing spaces at the end of lines.
 * @author Nodeca Team <https://github.com/nodeca>
 * @copyright 2015 Greg Cochard
 */
"use strict";

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = function(context) {

    var BLANK_CLASS = "[ \t\u00a0\u2000-\u200b\u2028\u2029\u3000]",
        SKIP_BLANK = "^" + BLANK_CLASS + "*$",
        NONBLANK = BLANK_CLASS + "$";

    var options = context.options[0] || {},
        skipBlankLines = options.skipBlankLines || false;


    //--------------------------------------------------------------------------
    // Public
    //--------------------------------------------------------------------------

    return {

        "Program": function checkTrailingSpaces(node) {

            // Let's hack. Since Espree does not return whitespace nodes,
            // fetch the source code and do matching via regexps.

            var src = context.getSource(),
                re = new RegExp(NONBLANK),
                skipMatch = new RegExp(SKIP_BLANK),
                matches, lines = src.split(/\r?\n/), location;

            for (var i = 0, ii = lines.length; i < ii; i++) {

                matches = re.exec(lines[i]);
                if (matches) {

                    // If the line has only whitespace, and skipBlankLines
                    // is true, don't report it
                    if (skipBlankLines && skipMatch.test(lines[i])) {
                        continue;
                    }
                    location = {
                        line: i + 1,
                        column: lines[i].length - matches[0].length + 1
                    };

                    // Passing node is a bit dirty, because message data will contain
                    // big text in `source`. But... who cares :) ?
                    // One more kludge will not make worse the bloody wizardry of this plugin.
                    context.report(node, location, "Trailing spaces not allowed.");
                }
            }
        }

    };
};

module.exports.schema = [
    {
        "type": "object",
        "properties": {
            "skipBlankLines": {
                "type": "boolean"
            }
        },
        "additionalProperties": false
    }
];
