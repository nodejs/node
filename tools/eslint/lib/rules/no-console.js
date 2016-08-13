/**
 * @fileoverview Rule to flag use of console object
 * @author Nicholas C. Zakas
 */

"use strict";

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = {
    meta: {
        docs: {
            description: "disallow the use of `console`",
            category: "Possible Errors",
            recommended: true
        },

        schema: [
            {
                type: "object",
                properties: {
                    allow: {
                        type: "array",
                        items: {
                            type: "string"
                        },
                        minItems: 1,
                        uniqueItems: true
                    }
                },
                additionalProperties: false
            }
        ]
    },

    create: function(context) {

        return {

            MemberExpression: function(node) {

                if (node.object.name === "console") {
                    let blockConsole = true;

                    if (context.options.length > 0) {
                        const allowedProperties = context.options[0].allow;
                        const passedProperty = node.property.name;
                        const propertyIsAllowed = (allowedProperties.indexOf(passedProperty) > -1);

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

    }
};
