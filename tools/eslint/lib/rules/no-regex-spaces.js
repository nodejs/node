/**
 * @fileoverview Rule to count multiple spaces in regular expressions
 * @author Matt DuVall <http://www.mattduvall.com/>
 */

"use strict";

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = {
    meta: {
        docs: {
            description: "disallow multiple spaces in regular expression literals",
            category: "Possible Errors",
            recommended: true
        },

        schema: []
    },

    create: function(context) {

        return {

            Literal: function(node) {
                var token = context.getFirstToken(node),
                    nodeType = token.type,
                    nodeValue = token.value,
                    multipleSpacesRegex = /( {2,})+?/,
                    regexResults;

                if (nodeType === "RegularExpression") {
                    regexResults = multipleSpacesRegex.exec(nodeValue);

                    if (regexResults !== null) {
                        context.report(node, "Spaces are hard to count. Use {" + regexResults[0].length + "}.");
                    }
                }
            }
        };

    }
};
