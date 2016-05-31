/**
 * @fileoverview Rule to flag when regex literals are not wrapped in parens
 * @author Matt DuVall <http://www.mattduvall.com>
 */

"use strict";

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = {
    meta: {
        docs: {
            description: "require parenthesis around regex literals",
            category: "Stylistic Issues",
            recommended: false
        },

        schema: []
    },

    create: function(context) {
        var sourceCode = context.getSourceCode();

        return {

            Literal: function(node) {
                var token = sourceCode.getFirstToken(node),
                    nodeType = token.type,
                    source,
                    grandparent,
                    ancestors;

                if (nodeType === "RegularExpression") {
                    source = sourceCode.getTokenBefore(node);
                    ancestors = context.getAncestors();
                    grandparent = ancestors[ancestors.length - 1];

                    if (grandparent.type === "MemberExpression" && grandparent.object === node &&
                        (!source || source.value !== "(")) {
                        context.report(node, "Wrap the regexp literal in parens to disambiguate the slash.");
                    }
                }
            }
        };

    }
};
