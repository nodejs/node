/**
 * @fileoverview Rule to check for ambiguous div operator in regexes
 * @author Matt DuVall <http://www.mattduvall.com>
 */

"use strict";

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = {
    meta: {
        docs: {
            description: "disallow division operators explicitly at the beginning of regular expressions",
            category: "Best Practices",
            recommended: false
        },

        schema: []
    },

    create: function(context) {

        return {

            Literal: function(node) {
                var token = context.getFirstToken(node);

                if (token.type === "RegularExpression" && token.value[1] === "=") {
                    context.report(node, "A regular expression literal can be confused with '/='.");
                }
            }
        };

    }
};
