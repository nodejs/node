/**
 * @fileoverview Rule to flag use of console object
 * @author Nicholas C. Zakas
 * @copyright 2016 Eric Correia. All rights reserved.
 */

"use strict";

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = function(context) {

    return {

        "MemberExpression": function(node) {

            if (node.object.name === "console") {
                var blockConsole = true;

                if (context.options.length > 0) {
                    var allowedProperties = context.options[0].allow;
                    var passedProperty = node.property.name;
                    var propertyIsAllowed = (allowedProperties.indexOf(passedProperty) > -1);

                    if (propertyIsAllowed) {
                        blockConsole = false;
                    }
                }

                if (blockConsole) {
                    context.report(node, "Unexpected console statement.");
                }
            }
        }
    };

};

module.exports.schema = [
    {
        "type": "object",
        "properties": {
            "allow": {
                "type": "array",
                "items": {
                    "type": "string"
                },
                "minItems": 1,
                "uniqueItems": true
            }
        },
        "additionalProperties": false
    }
];
