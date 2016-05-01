/**
 * @fileoverview Rule to disallow if as the only statmenet in an else block
 * @author Brandon Mills
 */
"use strict";

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = {
    meta: {
        docs: {
            description: "disallow `if` statements as the only statement in `else` blocks",
            category: "Stylistic Issues",
            recommended: false
        },

        schema: []
    },

    create: function(context) {

        return {
            IfStatement: function(node) {
                var ancestors = context.getAncestors(),
                    parent = ancestors.pop(),
                    grandparent = ancestors.pop();

                if (parent && parent.type === "BlockStatement" &&
                        parent.body.length === 1 && grandparent &&
                        grandparent.type === "IfStatement" &&
                        parent === grandparent.alternate) {
                    context.report(node, "Unexpected if as the only statement in an else block.");
                }
            }
        };

    }
};
