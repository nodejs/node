/**
 * @fileoverview Rule to enforce concise object methods and properties.
 * @author Jamund Ferguson
 * @copyright 2015 Jamund Ferguson. All rights reserved.
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
module.exports = function(context) {
    var APPLY = context.options[0] || OPTIONS.always;
    var APPLY_TO_METHODS = APPLY === OPTIONS.methods || APPLY === OPTIONS.always;
    var APPLY_TO_PROPS = APPLY === OPTIONS.properties || APPLY === OPTIONS.always;
    var APPLY_NEVER = APPLY === OPTIONS.never;

    var PARAMS = context.options[1] || {};
    var IGNORE_CONSTRUCTORS = PARAMS.ignoreConstructors;

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

    //--------------------------------------------------------------------------
    // Public
    //--------------------------------------------------------------------------

    return {
        "Property": function(node) {
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

            // at this point if we're concise or if we're "never" we can leave
            if (APPLY_NEVER || isConciseProperty) {
                return;
            }

            // getters, setters and computed properties are ignored
            if (node.kind === "get" || node.kind === "set" || node.computed) {
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

};

module.exports.schema = {
    "anyOf": [
        {
            "type": "array",
            "items": [
                {
                    "enum": ["always", "methods", "properties", "never"]
                }
            ],
            "minItems": 0,
            "maxItems": 1
        },
        {
            "type": "array",
            "items": [
                {
                    "enum": ["always", "methods"]
                },
                {
                    "type": "object",
                    "properties": {
                        "ignoreConstructors": {
                            "type": "boolean"
                        }
                    },
                    "additionalProperties": false
                }
            ],
            "minItems": 0,
            "maxItems": 2
        }
    ]
};
