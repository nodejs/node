/**
 * @fileoverview Rule to flag use of parseInt without a radix argument
 * @author James Allardice
 */

"use strict";

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = function(context) {

    return {
        "CallExpression": function(node) {

            var radix;

            if (node.callee.name === "parseInt") {

                if (node.arguments.length < 2) {
                    context.report(node, "Missing radix parameter.");
                } else {

                    radix = node.arguments[1];

                    // don't allow non-numeric literals or undefined
                    if ((radix.type === "Literal" && typeof radix.value !== "number") ||
                        (radix.type === "Identifier" && radix.name === "undefined")
                    ) {
                        context.report(node, "Invalid radix parameter.");
                    }
                }

            }
        }
    };

};
