/**
 * @fileoverview Ensures that the results of typeof are compared against a valid string
 * @author Ian Christian Myers
 */
"use strict";

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = function(context) {

    var VALID_TYPES = ["symbol", "undefined", "object", "boolean", "number", "string", "function"],
        OPERATORS = ["==", "===", "!=", "!=="];

    //--------------------------------------------------------------------------
    // Public
    //--------------------------------------------------------------------------

    return {

        "UnaryExpression": function(node) {
            var parent, sibling;

            if (node.operator === "typeof") {
                parent = context.getAncestors().pop();

                if (parent.type === "BinaryExpression" && OPERATORS.indexOf(parent.operator) !== -1) {
                    sibling = parent.left === node ? parent.right : parent.left;

                    if (sibling.type === "Literal" && VALID_TYPES.indexOf(sibling.value) === -1) {
                        context.report(sibling, "Invalid typeof comparison value");
                    }
                }
            }
        }

    };

};

module.exports.schema = [];
