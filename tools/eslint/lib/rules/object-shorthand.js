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

        fixable: "code",

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

                // getters and setters are ignored
                if (node.kind === "get" || node.kind === "set") {
                    return;
                }

                // only computed methods can fail the following checks
                if (node.computed && node.value.type !== "FunctionExpression") {
                    return;
                }

                //--------------------------------------------------------------
                // Checks for property/method shorthand.
                if (isConciseProperty) {

                    // if we're "never" and concise we should warn now
                    if (APPLY_NEVER) {
                        type = node.method ? "method" : "property";
                        context.report({
                            node: node,
                            message: "Expected longform " + type + " syntax.",
                            fix: function(fixer) {
                                if (node.method) {
                                    if (node.value.generator) {
                                        return fixer.replaceTextRange([node.range[0], node.key.range[1]], node.key.name + ": function*");
                                    }

                                    return fixer.insertTextAfter(node.key, ": function");
                                }

                                return fixer.insertTextAfter(node.key, ": " + node.key.name);
                            }
                        });
                    }

                    // {'xyz'() {}} should be written as {'xyz': function() {}}
                    if (AVOID_QUOTES && isStringLiteral(node.key)) {
                        context.report({
                            node: node,
                            message: "Expected longform method syntax for string literal keys.",
                            fix: function(fixer) {
                                if (node.computed) {
                                    return fixer.insertTextAfterRange([node.key.range[0], node.key.range[1] + 1], ": function");
                                }

                                return fixer.insertTextAfter(node.key, ": function");
                            }
                        });
                    }

                    return;
                }

                //--------------------------------------------------------------
                // Checks for longform properties.
                if (node.value.type === "FunctionExpression" && !node.value.id && APPLY_TO_METHODS) {
                    if (IGNORE_CONSTRUCTORS && isConstructor(node.key.name)) {
                        return;
                    }
                    if (AVOID_QUOTES && isStringLiteral(node.key)) {
                        return;
                    }

                    // {[x]: function(){}} should be written as {[x]() {}}
                    if (node.computed) {
                        context.report({
                            node: node,
                            message: "Expected method shorthand.",
                            fix: function(fixer) {
                                if (node.value.generator) {
                                    return fixer.replaceTextRange(
                                        [node.key.range[0], node.value.range[0] + "function*".length],
                                        "*[" + node.key.name + "]"
                                    );
                                }

                                return fixer.removeRange([node.key.range[1] + 1, node.value.range[0] + "function".length]);
                            }
                        });
                        return;
                    }

                    // {x: function(){}} should be written as {x() {}}
                    context.report({
                        node: node,
                        message: "Expected method shorthand.",
                        fix: function(fixer) {
                            if (node.value.generator) {
                                return fixer.replaceTextRange(
                                    [node.key.range[0], node.value.range[0] + "function*".length],
                                    "*" + node.key.name
                                );
                            }

                            return fixer.removeRange([node.key.range[1], node.value.range[0] + "function".length]);
                        }
                    });
                } else if (node.value.type === "Identifier" && node.key.name === node.value.name && APPLY_TO_PROPS) {

                    // {x: x} should be written as {x}
                    context.report({
                        node: node,
                        message: "Expected property shorthand.",
                        fix: function(fixer) {
                            return fixer.replaceText(node, node.value.name);
                        }
                    });
                } else if (node.value.type === "Identifier" && node.key.type === "Literal" && node.key.value === node.value.name && APPLY_TO_PROPS) {
                    if (AVOID_QUOTES) {
                        return;
                    }

                    // {"x": x} should be written as {x}
                    context.report({
                        node: node,
                        message: "Expected property shorthand.",
                        fix: function(fixer) {
                            return fixer.replaceText(node, node.value.name);
                        }
                    });
                }
            }
        };
    }
};
