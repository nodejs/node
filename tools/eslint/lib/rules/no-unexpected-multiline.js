/**
 * @fileoverview Rule to spot scenarios where a newline looks like it is ending a statement, but is not.
 * @author Glen Mailer
 * @copyright 2015 Glen Mailer
 */
"use strict";

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------
module.exports = function(context) {

    var FUNCTION_MESSAGE = "Unexpected newline between function and ( of function call.";
    var PROPERTY_MESSAGE = "Unexpected newline between object and [ of property access.";

    /**
     * Check to see if the bracket prior to the node is continuing the previous
     * line's expression
     * @param {ASTNode} node The node to check.
     * @param {string} msg The error message to use.
     * @returns {void}
     * @private
     */
    function checkForBreakBefore(node, msg) {
        var tokens = context.getTokensBefore(node, 2);
        var paren = tokens[1];
        var before = tokens[0];
        if (paren.loc.start.line !== before.loc.end.line) {
            context.report(node, paren.loc.start, msg, { char: paren.value });
        }
    }

    //--------------------------------------------------------------------------
    // Public API
    //--------------------------------------------------------------------------

    return {

        "MemberExpression": function(node) {
            if (!node.computed) {
                return;
            }

            checkForBreakBefore(node.property, PROPERTY_MESSAGE);
        },

        "CallExpression": function(node) {
            if (node.arguments.length === 0) {
                return;
            }

            checkForBreakBefore(node.arguments[0], FUNCTION_MESSAGE);
        }
    };

};

module.exports.schema = [];
