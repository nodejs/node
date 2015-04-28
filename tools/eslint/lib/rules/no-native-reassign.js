/**
 * @fileoverview Rule to flag when re-assigning native objects
 * @author Ilya Volodin
 */

"use strict";

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = function(context) {

    var nativeObjects = ["Array", "Boolean", "Date", "decodeURI",
                        "decodeURIComponent", "encodeURI", "encodeURIComponent",
                        "Error", "eval", "EvalError", "Function", "isFinite",
                        "isNaN", "JSON", "Math", "Number", "Object", "parseInt",
                        "parseFloat", "RangeError", "ReferenceError", "RegExp",
                        "String", "SyntaxError", "TypeError", "URIError",
                        "Map", "NaN", "Set", "WeakMap", "Infinity", "undefined"];

    return {

        "AssignmentExpression": function(node) {
            if (nativeObjects.indexOf(node.left.name) >= 0) {
                context.report(node, node.left.name + " is a read-only native object.");
            }
        },

        "VariableDeclarator": function(node) {
            if (nativeObjects.indexOf(node.id.name) >= 0) {
                context.report(node, "Redefinition of '{{nativeObject}}'.", { nativeObject: node.id.name });
            }
        }
    };

};
