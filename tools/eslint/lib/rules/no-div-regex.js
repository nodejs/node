/**
 * @fileoverview Rule to check for ambiguous div operator in regexes
 * @author Matt DuVall <http://www.mattduvall.com>
 */

"use strict";

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = function(context) {

    return {

        "Literal": function(node) {
            var token = context.getFirstToken(node);

            if (token.type === "RegularExpression" && token.value[1] === "=") {
                context.report(node, "A regular expression literal can be confused with '/='.");
            }
        }
    };

};

module.exports.schema = [];
