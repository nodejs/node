/**
 * @fileoverview Rule to flag trailing commas in object literals.
 * @author Ian Christian Myers
 */

"use strict";

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = function(context) {

     //-------------------------------------------------------------------------
     // Helpers
     //-------------------------------------------------------------------------

    function checkForTrailingComma(node) {
        var items = node.properties || node.elements,
            length = items.length,
            lastItem, penultimateToken;

        if (length) {
            lastItem = items[length - 1];
            if (lastItem) {
                penultimateToken = context.getLastToken(node, 1);
                if (penultimateToken.value === ",") {
                    context.report(lastItem, penultimateToken.loc.start, "Trailing comma.");
                }
            }
        }
    }

    //--------------------------------------------------------------------------
    // Public API
    //--------------------------------------------------------------------------

    return {
        "ObjectExpression": checkForTrailingComma,
        "ArrayExpression": checkForTrailingComma
    };

};
