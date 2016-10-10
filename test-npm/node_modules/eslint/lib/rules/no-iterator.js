/**
 * @fileoverview Rule to flag usage of __iterator__ property
 * @author Ian Christian Myers
 */

"use strict";

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = function(context) {

    return {

        "MemberExpression": function(node) {

            if (node.property &&
                    (node.property.type === "Identifier" && node.property.name === "__iterator__" && !node.computed) ||
                    (node.property.type === "Literal" && node.property.value === "__iterator__")) {
                context.report(node, "Reserved name '__iterator__'.");
            }
        }
    };

};

module.exports.schema = [];
