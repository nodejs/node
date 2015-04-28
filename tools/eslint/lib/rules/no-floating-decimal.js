/**
 * @fileoverview Rule to flag use of a leading/trailing decimal point in a numeric literal
 * @author James Allardice
 */

"use strict";

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = function(context) {

    return {
        "Literal": function(node) {

            if (typeof node.value === "number") {
                if (node.raw.indexOf(".") === 0) {
                    context.report(node, "A leading decimal point can be confused with a dot.");
                }
                if (node.raw.indexOf(".") === node.raw.length - 1) {
                    context.report(node, "A trailing decimal point can be confused with a dot.");
                }
            }
        }
    };

};
