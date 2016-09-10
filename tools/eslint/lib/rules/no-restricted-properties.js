/**
 * @fileoverview Rule to disallow certain object properties
 * @author Will Klein & Eli White
 */

"use strict";

const astUtils = require("../ast-utils");

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = {
    meta: {
        docs: {
            description: "disallow certain properties on certain objects",
            category: "Node.js and CommonJS",
            recommended: false
        },

        schema: {
            type: "array",
            items: {
                type: "object",
                properties: {
                    object: {
                        type: "string"
                    },
                    property: {
                        type: "string"
                    },
                    message: {
                        type: "string"
                    }
                },
                additionalProperties: false,
                required: [
                    "object",
                    "property"
                ]
            },
            uniqueItems: true
        }
    },

    create(context) {
        const restrictedCalls = context.options;

        if (restrictedCalls.length === 0) {
            return {};
        }

        const restrictedProperties = restrictedCalls.reduce(function(restrictions, option) {
            const objectName = option.object;
            const propertyName = option.property;

            if (!restrictions.has(objectName)) {
                restrictions.set(objectName, new Map());
            }

            restrictions.get(objectName).set(propertyName, {
                message: option.message
            });

            return restrictions;
        }, new Map());

        return {
            MemberExpression(node) {
                const objectName = node.object && node.object.name;
                const propertyName = astUtils.getStaticPropertyName(node);
                const matchedObject = restrictedProperties.get(objectName);
                const matchedObjectProperty = matchedObject && matchedObject.get(propertyName);

                if (matchedObjectProperty) {
                    const message = matchedObjectProperty.message ? " " + matchedObjectProperty.message : "";

                    context.report(node, "'{{objectName}}.{{propertyName}}' is restricted from being used.{{message}}", {
                        objectName,
                        propertyName,
                        message
                    });
                }
            }
        };
    }
};
