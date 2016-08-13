/**
 * @fileoverview Disallow sparse arrays
 * @author Nicholas C. Zakas
 */
"use strict";

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = {
    meta: {
        docs: {
            description: "disallow sparse arrays",
            category: "Possible Errors",
            recommended: true
        },

        schema: []
    },

    create: function(context) {


        //--------------------------------------------------------------------------
        // Public
        //--------------------------------------------------------------------------

        return {

            ArrayExpression: function(node) {

                const emptySpot = node.elements.indexOf(null) > -1;

                if (emptySpot) {
                    context.report(node, "Unexpected comma in middle of array.");
                }
            }

        };

    }
};
