/**
 * @fileoverview Rule to flag use of duplicate keys in an object.
 * @author Ian Christian Myers
 * @copyright 2013 Ian Christian Myers. All rights reserved.
 * @copyright 2013 Nicholas C. Zakas. All rights reserved.
 */

"use strict";

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = function(context) {

    return {

        "ObjectExpression": function(node) {

            // Object that will be a map of properties--safe because we will
            // prefix all of the keys.
            var nodeProps = Object.create(null);

            node.properties.forEach(function(property) {
                var keyName = property.key.name || property.key.value,
                    key = property.kind + "-" + keyName,
                    checkProperty = (!property.computed || property.key.type === "Literal");

                if (checkProperty) {
                    if (nodeProps[key]) {
                        context.report(node, "Duplicate key '{{key}}'.", { key: keyName });
                    } else {
                        nodeProps[key] = true;
                    }
                }
            });

        }
    };

};

module.exports.schema = [];
