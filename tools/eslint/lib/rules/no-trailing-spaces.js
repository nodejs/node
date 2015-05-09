/**
 * @fileoverview Disallow trailing spaces at the end of lines.
 * @author Nodeca Team <https://github.com/nodeca>
 */
"use strict";

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = function(context) {

    var TRAILER = "[ \t\u00a0\u2000\u2001\u2002\u2003\u2004\u2005\u2006\u2007\u2008\u2009\u200a\u200b\u2028\u2029\u3000]+$";

    //--------------------------------------------------------------------------
    // Public
    //--------------------------------------------------------------------------

    return {

        "Program": function checkTrailingSpaces(node) {

            // Let's hack. Since Esprima does not return whitespace nodes,
            // fetch the source code and do black magic via regexps.

            var src = context.getSource(),
                re = new RegExp(TRAILER, "mg"),
                match, lines, location;

            while ((match = re.exec(src)) !== null) {
                lines = src.slice(0, re.lastIndex).split(/\r?\n/g);

                location = {
                    line: lines.length,
                    column: lines[lines.length - 1].length - match[0].length + 1
                };

                // Passing node is a bit dirty, because message data will contain
                // big text in `source`. But... who cares :) ?
                // One more kludge will not make worse the bloody wizardry of this plugin.
                context.report(node, location, "Trailing spaces not allowed.");
            }
        }

    };
};
