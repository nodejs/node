/**
 * @fileoverview Disallow Labeled Statements
 * @author Nicholas C. Zakas
 */
"use strict";

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = function(context) {

    //--------------------------------------------------------------------------
    // Public
    //--------------------------------------------------------------------------

    return {

        "LabeledStatement": function(node) {
            context.report(node, "Unexpected labeled statement.");
        },

        "BreakStatement": function(node) {

            if (node.label) {
                context.report(node, "Unexpected label in break statement.");
            }

        },

        "ContinueStatement": function(node) {

            if (node.label) {
                context.report(node, "Unexpected label in continue statement.");
            }

        }


    };

};
