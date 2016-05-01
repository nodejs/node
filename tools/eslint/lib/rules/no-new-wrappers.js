/**
 * @fileoverview Rule to flag when using constructor for wrapper objects
 * @author Ilya Volodin
 */

"use strict";

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = {
    meta: {
        docs: {
            description: "disallow `new` operators with the `String`, `Number`, and `Boolean` objects",
            category: "Best Practices",
            recommended: false
        },

        schema: []
    },

    create: function(context) {

        return {

            NewExpression: function(node) {
                var wrapperObjects = ["String", "Number", "Boolean", "Math", "JSON"];

                if (wrapperObjects.indexOf(node.callee.name) > -1) {
                    context.report(node, "Do not use {{fn}} as a constructor.", { fn: node.callee.name });
                }
            }
        };

    }
};
