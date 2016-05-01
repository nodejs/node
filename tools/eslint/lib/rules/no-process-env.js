/**
 * @fileoverview Disallow the use of process.env()
 * @author Vignesh Anand
 */
"use strict";

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = {
    meta: {
        docs: {
            description: "disallow the use of `process.env`",
            category: "Node.js and CommonJS",
            recommended: false
        },

        schema: []
    },

    create: function(context) {

        return {

            MemberExpression: function(node) {
                var objectName = node.object.name,
                    propertyName = node.property.name;

                if (objectName === "process" && !node.computed && propertyName && propertyName === "env") {
                    context.report(node, "Unexpected use of process.env.");
                }

            }

        };

    }
};
