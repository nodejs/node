/**
 * @fileoverview Rule to enforce concise object methods and properties.
 * @author Jamund Ferguson
 */

"use strict";

var OPTIONS = {
    always: "always",
    never: "never",
    methods: "methods",
    properties: "properties"
};

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------
module.exports = {
    meta: {
        docs: {
            description: "require or disallow method and property shorthand syntax for object literals",
            category: "ECMAScript 6",
            recommended: false
        },

        schema: {
            anyOf: [
                {
                    type: "array",
                    items: [
                        {
                            enum: ["always", "methods", "properties", "never"]
                        }
                    ],
                    minItems: 0,
                    maxItems: 1
                },
                {
                    type: "array",
                    items: [
                        {
                            enum: ["always", "methods", "properties"]
                        },
                        {
                            type: "object",
                            properties: {
                                avoidQuotes: {
                                    type: "boolean"
                                }
                            },
                            additionalProperties: false
                        }
                    ],
                    minItems: 0,
                    maxItems: 2
                },
                {
                    type: "array",
                    items: [
                        {
                            enum: ["always", "methods"]
                        },
                        {
                            type: "object",
                            properties: {
                                ignoreConstructors: {
                                    type: "boolean"
                                },
                                avoidQuotes: {
                                    type: "boolean"
                                }
                            },
                            additionalProperties: false
                        }
                    ],
                    minItems: 0,
                    maxItems: 2
                }
            ]
        }
    },

    create: function(context) {
        var APPLY = context.options[0] || OPTIONS.always;
        var APPLY_TO_METHODS = APPLY === OPTIONS.methods || APPLY === OPTIONS.always;
        var APPLY_TO_PROPS = APPLY === OPTIONS.properties || APPLY === OPTIONS.always;
        var APPLY_NEVER = APPLY === OPTIONS.never;

        var PARAMS = context.options[1] || {};
        var IGNORE_CONSTRUCTORS = PARAMS.ignoreConstructors;
        var AVOID_QUOTES = PARAMS.avoidQuotes;

        //--------------------------------------------------------------------------
        // Helpers
        //--------------------------------------------------------------------------

        /**
         * Determines if the first character of the name is a capital letter.
         * @param {string} name The name of the node to evaluate.
         * @returns {boolean} True if the first character of the property name is a capital letter, false if not.
         * @private
         */
        function isConstructor(name) {
            var firstChar = name.charAt(0);

            return firstChar === firstChar.toUpperCase();
        }

        /**
          * Checks whether a node is a string literal.
          * @param   {ASTNode} node - Any AST node.
          * @returns {boolean} `true` if it is a string literal.
          */
        function isStringLiteral(node) {
            return node.type === "Literal" && typeof node.value === "string";
        }

        //--------------------------------------------------------------------------
        // Public
        //--------------------------------------------------------------------------

        return {
            Property: function(node) {
                var isConciseProperty = node.method || node.shorthand,
                    type;

                // Ignore destructuring assignment
                if (node.parent.type === "ObjectPattern") {
                    return;
                }

                // if we're "never" and concise we should warn now
                if (APPLY_NEVER && isConciseProperty) {
                    type = node.method ? "method" : "property";
                    context.report(node, "Expected longform " + type + " syntax.");
                }

                // {'xyz'() {}} should be written as {'xyz': function() {}}
                if (AVOID_QUOTES && isStringLiteral(node.key) && isConciseProperty) {
                    context.report(node, "Expected longform method syntax for string literal keys.");
                }

                // at this point if we're concise or if we're "never" we can leave
                if (APPLY_NEVER || AVOID_QUOTES || isConciseProperty) {
                    return;
                }

                // only computed methods can fail the following checks
                if (node.computed && node.value.type !== "FunctionExpression") {
                    return;
                }

                // getters and setters are ignored
                if (node.kind === "get" || node.kind === "set") {
                    return;
                }

                if (node.value.type === "FunctionExpression" && !node.value.id && APPLY_TO_METHODS) {
                    if (IGNORE_CONSTRUCTORS && isConstructor(node.key.name)) {
                        return;
                    }

                    // {x: function(){}} should be written as {x() {}}
                    context.report(node, "Expected method shorthand.");
                } else if (node.value.type === "Identifier" && node.key.name === node.value.name && APPLY_TO_PROPS) {

                    // {x: x} should be written as {x}
                    context.report(node, "Expected property shorthand.");
                } else if (node.value.type === "Identifier" && node.key.type === "Literal" && node.key.value === node.value.name && APPLY_TO_PROPS) {

                    // {"x": x} should be written as {x}
                    context.report(node, "Expected property shorthand.");
                }
            }
        };
    }
};
