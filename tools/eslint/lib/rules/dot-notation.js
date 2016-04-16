/**
 * @fileoverview Rule to warn about using dot notation instead of square bracket notation when possible.
 * @author Josh Perez
 */
"use strict";

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

var validIdentifier = /^[a-zA-Z_$][a-zA-Z0-9_$]*$/;
var keywords = require("../util/keywords");

module.exports = function(context) {
    var options = context.options[0] || {};
    var allowKeywords = options.allowKeywords === void 0 || !!options.allowKeywords;

    var allowPattern;

    if (options.allowPattern) {
        allowPattern = new RegExp(options.allowPattern);
    }

    return {
        "MemberExpression": function(node) {
            if (
                node.computed &&
                node.property.type === "Literal" &&
                validIdentifier.test(node.property.value) &&
                (allowKeywords || keywords.indexOf("" + node.property.value) === -1)
            ) {
                if (!(allowPattern && allowPattern.test(node.property.value))) {
                    context.report(node.property, "[" + JSON.stringify(node.property.value) + "] is better written in dot notation.");
                }
            }
            if (
                !allowKeywords &&
                !node.computed &&
                keywords.indexOf("" + node.property.name) !== -1
            ) {
                context.report(node.property, "." + node.property.name + " is a syntax error.");
            }
        }
    };
};

module.exports.schema = [
    {
        "type": "object",
        "properties": {
            "allowKeywords": {
                "type": "boolean"
            },
            "allowPattern": {
                "type": "string"
            }
        },
        "additionalProperties": false
    }
];
