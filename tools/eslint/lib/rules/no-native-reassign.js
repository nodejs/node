/**
 * @fileoverview Rule to flag when re-assigning native objects
 * @author Ilya Volodin
 */

"use strict";

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = function(context) {

    var NATIVE_OBJECTS = ["Array", "Boolean", "Date", "decodeURI",
                        "decodeURIComponent", "encodeURI", "encodeURIComponent",
                        "Error", "eval", "EvalError", "Function", "isFinite",
                        "isNaN", "JSON", "Math", "Number", "Object", "parseInt",
                        "parseFloat", "RangeError", "ReferenceError", "RegExp",
                        "String", "SyntaxError", "TypeError", "URIError",
                        "Map", "NaN", "Set", "WeakMap", "Infinity", "undefined"];
    var config = context.options[0] || {};
    var exceptions = config.exceptions || [];
    var modifiedNativeObjects = NATIVE_OBJECTS;

    if (exceptions.length) {
        modifiedNativeObjects = NATIVE_OBJECTS.filter(function(builtIn) {
            return exceptions.indexOf(builtIn) === -1;
        });
    }

    return {

        "AssignmentExpression": function(node) {
            if (modifiedNativeObjects.indexOf(node.left.name) >= 0) {
                context.report(node, node.left.name + " is a read-only native object.");
            }
        },

        "VariableDeclarator": function(node) {
            if (modifiedNativeObjects.indexOf(node.id.name) >= 0) {
                context.report(node, "Redefinition of '{{nativeObject}}'.", { nativeObject: node.id.name });
            }
        }
    };

};

module.exports.schema = [
    {
        "type": "object",
        "properties": {
            "exceptions": {
                "type": "array",
                "items": {
                    "type": "string"
                },
                "uniqueItems": true
            }
        },
        "additionalProperties": false
    }
];
