/**
 * @fileoverview Rule to enforce concise object methods and properties.
 * @author Jamund Ferguson
 */

"use strict";

const OPTIONS = {
    always: "always",
    never: "never",
    methods: "methods",
    properties: "properties",
    consistent: "consistent",
    consistentAsNeeded: "consistent-as-needed"
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
                            enum: ["always", "methods", "properties", "never", "consistent", "consistent-as-needed"]
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
        const APPLY = context.options[0] || OPTIONS.always;
        const APPLY_TO_METHODS = APPLY === OPTIONS.methods || APPLY === OPTIONS.always;
        const APPLY_TO_PROPS = APPLY === OPTIONS.properties || APPLY === OPTIONS.always;
        const APPLY_NEVER = APPLY === OPTIONS.never;
        const APPLY_CONSISTENT = APPLY === OPTIONS.consistent;
        const APPLY_CONSISTENT_AS_NEEDED = APPLY === OPTIONS.consistentAsNeeded;

        const PARAMS = context.options[1] || {};
        const IGNORE_CONSTRUCTORS = PARAMS.ignoreConstructors;
        const AVOID_QUOTES = PARAMS.avoidQuotes;

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
            const firstChar = name.charAt(0);

            return firstChar === firstChar.toUpperCase();
        }

        /**
         * Determines if the property is not a getter and a setter.
         * @param {ASTNode} property Property AST node
         * @returns {boolean} True if the property is not a getter and a setter, false if it is.
         * @private
         **/
        function isNotGetterOrSetter(property) {
            return (property.kind !== "set" && property.kind !== "get");
        }

        /**
          * Checks whether a node is a string literal.
          * @param   {ASTNode} node - Any AST node.
          * @returns {boolean} `true` if it is a string literal.
          */
        function isStringLiteral(node) {
            return node.type === "Literal" && typeof node.value === "string";
        }

        /**
         * Determines if the property is a shorthand or not.
         * @param {ASTNode} property Property AST node
         * @returns {boolean} True if the property is considered shorthand, false if not.
         * @private
         **/
        function isShorthand(property) {

            // property.method is true when `{a(){}}`.
            return (property.shorthand || property.method);
        }

        /**
         * Determines if the property's key and method or value are named equally.
         * @param {ASTNode} property Property AST node
         * @returns {boolean} True if the key and value are named equally, false if not.
         * @private
         **/
        function isRedudant(property) {
            return (property.key && (

                // A function expression
                property.value && property.value.id && property.value.id.name === property.key.name ||

                // A property
                property.value && property.value.name === property.key.name
            ));
        }

        /**
         * Ensures that an object's properties are consistently shorthand, or not shorthand at all.
         * @param   {ASTNode} node Property AST node
         * @param   {boolean} checkRedundancy Whether to check longform redundancy
         * @returns {void}
         **/
        function checkConsistency(node, checkRedundancy) {

            // We are excluding getters and setters as they are considered neither longform nor shorthand.
            const properties = node.properties.filter(isNotGetterOrSetter);

            // Do we still have properties left after filtering the getters and setters?
            if (properties.length > 0) {
                const shorthandProperties = properties.filter(isShorthand);

                // If we do not have an equal number of longform properties as
                // shorthand properties, we are using the annotations inconsistently
                if (shorthandProperties.length !== properties.length) {

                    // We have at least 1 shorthand property
                    if (shorthandProperties.length > 0) {
                        context.report(node, "Unexpected mix of shorthand and non-shorthand properties.");
                    } else if (checkRedundancy) {

                        // If all properties of the object contain a method or value with a name matching it's key,
                        // all the keys are redudant.
                        const canAlwaysUseShorthand = properties.every(isRedudant);

                        if (canAlwaysUseShorthand) {
                            context.report(node, "Expected shorthand for all properties.");
                        }
                    }
                }
            }
        }

        //--------------------------------------------------------------------------
        // Public
        //--------------------------------------------------------------------------

        return {
            ObjectExpression: function(node) {
                if (APPLY_CONSISTENT) {
                    checkConsistency(node, false);
                } else if (APPLY_CONSISTENT_AS_NEEDED) {
                    checkConsistency(node, true);
                }
            },

            Property: function(node) {
                const isConciseProperty = node.method || node.shorthand;

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
                        const type = node.method ? "method" : "property";

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
