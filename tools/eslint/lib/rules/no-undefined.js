/**
 * @fileoverview Rule to flag references to the undefined variable.
 * @author Michael Ficarra
 */
"use strict";

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = function(context) {

    return {

        "Identifier": function(node) {
            if (node.name === "undefined") {
                var parent = context.getAncestors().pop();
                if (!parent || parent.type !== "MemberExpression" || node !== parent.property || parent.computed) {
                    context.report(node, "Unexpected use of undefined.");
                }
            }
        }
    };

};

module.exports.schema = [];
