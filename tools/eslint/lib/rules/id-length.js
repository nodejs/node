/**
 * @fileoverview Rule that warns when identifier names are shorter or longer
 * than the values provided in configuration.
 * @author Burak Yigit Kaya aka BYK
 */

"use strict";

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = {
    meta: {
        docs: {
            description: "enforce minimum and maximum identifier lengths",
            category: "Stylistic Issues",
            recommended: false
        },

        schema: [
            {
                type: "object",
                properties: {
                    min: {
                        type: "number"
                    },
                    max: {
                        type: "number"
                    },
                    exceptions: {
                        type: "array",
                        uniqueItems: true,
                        items: {
                            type: "string"
                        }
                    },
                    properties: {
                        enum: ["always", "never"]
                    }
                },
                additionalProperties: false
            }
        ]
    },

    create: function(context) {
        let options = context.options[0] || {};
        let minLength = typeof options.min !== "undefined" ? options.min : 2;
        let maxLength = typeof options.max !== "undefined" ? options.max : Infinity;
        let properties = options.properties !== "never";
        let exceptions = (options.exceptions ? options.exceptions : [])
            .reduce(function(obj, item) {
                obj[item] = true;

                return obj;
            }, {});

        let SUPPORTED_EXPRESSIONS = {
            MemberExpression: properties && function(parent) {
                return !parent.computed && (

                    // regular property assignment
                    (parent.parent.left === parent || // or the last identifier in an ObjectPattern destructuring
                    parent.parent.type === "Property" && parent.parent.value === parent &&
                    parent.parent.parent.type === "ObjectPattern" && parent.parent.parent.parent.left === parent.parent.parent)
                );
            },
            AssignmentPattern: function(parent, node) {
                return parent.left === node;
            },
            VariableDeclarator: function(parent, node) {
                return parent.id === node;
            },
            Property: properties && function(parent, node) {
                return parent.key === node;
            },
            ImportDefaultSpecifier: true,
            RestElement: true,
            FunctionExpression: true,
            ArrowFunctionExpression: true,
            ClassDeclaration: true,
            FunctionDeclaration: true,
            MethodDefinition: true,
            CatchClause: true
        };

        return {
            Identifier: function(node) {
                let name = node.name;
                let parent = node.parent;

                let isShort = name.length < minLength;
                let isLong = name.length > maxLength;

                if (!(isShort || isLong) || exceptions[name]) {
                    return;  // Nothing to report
                }

                let isValidExpression = SUPPORTED_EXPRESSIONS[parent.type];

                if (isValidExpression && (isValidExpression === true || isValidExpression(parent, node))) {
                    context.report(
                        node,
                        isShort ?
                            "Identifier name '{{name}}' is too short (< {{min}})." :
                            "Identifier name '{{name}}' is too long (> {{max}}).",
                        { name: name, min: minLength, max: maxLength }
                    );
                }
            }
        };
    }
};
