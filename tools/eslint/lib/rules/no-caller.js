/**
 * @fileoverview Rule to flag use of arguments.callee and arguments.caller.
 * @author Nicholas C. Zakas
 */

"use strict";

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = {
    meta: {
        docs: {
            description: "disallow the use of `arguments.caller` or `arguments.callee`",
            category: "Best Practices",
            recommended: false
        },

        schema: []
    },

    create: function(context) {

        return {

            MemberExpression: function(node) {
                var objectName = node.object.name,
                    propertyName = node.property.name;

                if (objectName === "arguments" && !node.computed && propertyName && propertyName.match(/^calle[er]$/)) {
                    context.report(node, "Avoid arguments.{{property}}.", { property: propertyName });
                }

            }
        };

    }
};
