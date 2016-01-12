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

    //--------------------------------------------------------------------------
    // Public
    //--------------------------------------------------------------------------

    return {
        "Property": function(node) {
            var isConciseProperty = node.method || node.shorthand,
                type;

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

module.exports.schema = [
    {
        "enum": ["always", "methods", "properties", "never"]
    }
];
