/**
 * @fileoverview Rule to flag use of duplicate keys in an object.
 * @author Ian Christian Myers
 */

"use strict";

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = {
    meta: {
        docs: {
            description: "disallow duplicate keys in object literals",
            category: "Possible Errors",
            recommended: true
        },

        schema: []
    },

    create: function(context) {

        return {

            ObjectExpression: function(node) {

                // Object that will be a map of properties--safe because we will
                // prefix all of the keys.
                var nodeProps = Object.create(null);

                node.properties.forEach(function(property) {

                    if (property.type !== "Property") {
                        return;
                    }

                    var keyName = property.key.name || property.key.value,
                        key = property.kind + "-" + keyName,
                        checkProperty = (!property.computed || property.key.type === "Literal");

                    if (checkProperty) {
                        if (nodeProps[key]) {
                            context.report(node, property.loc.start, "Duplicate key '{{key}}'.", { key: keyName });
                        } else {
                            nodeProps[key] = true;
                        }
                    }
                });

            }
        };

    }
};
