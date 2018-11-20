/**
 * @fileoverview Rule to flag when regex literals are not wrapped in parens
 * @author Matt DuVall <http://www.mattduvall.com>
 */

"use strict";

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = function(context) {

    return {

        "Literal": function(node) {
            var token = context.getFirstToken(node),
                nodeType = token.type,
                source,
                grandparent,
                ancestors;

            if (nodeType === "RegularExpression") {
                source = context.getTokenBefore(node);
                ancestors = context.getAncestors();
                grandparent = ancestors[ancestors.length - 1];

                if (grandparent.type === "MemberExpression" && grandparent.object === node &&
                    (!source || source.value !== "(")) {
                    context.report(node, "Wrap the regexp literal in parens to disambiguate the slash.");
                }
            }
        }
    };

};
