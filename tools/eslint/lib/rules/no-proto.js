/**
 * @fileoverview Rule to flag usage of __proto__ property
 * @author Ilya Volodin
 */

"use strict";

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = function(context) {

    return {

        "MemberExpression": function(node) {

            if (node.property &&
                    (node.property.type === "Identifier" && node.property.name === "__proto__" && !node.computed) ||
                    (node.property.type === "Literal" && node.property.value === "__proto__")) {
                context.report(node, "The '__proto__' property is deprecated.");
            }
        }
    };

};
