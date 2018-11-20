/**
 * @fileoverview Rule to flag nested ternary expressions
 * @author Ian Christian Myers
 */

"use strict";

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = function(context) {

    return {
        "ConditionalExpression": function(node) {
            if (node.alternate.type === "ConditionalExpression" ||
                    node.consequent.type === "ConditionalExpression") {
                context.report(node, "Do not nest ternary expressions");
            }
        }
    };
};
