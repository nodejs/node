/**
 * @fileoverview Rule that warns when identifier names are shorter or longer
 *               than the values provided in configuration.
 * @author Burak Yigit Kaya aka BYK
 * @copyright 2015 Burak Yigit Kaya. All rights reserved.
 * @copyright 2015 Mathieu M-Gosselin. All rights reserved.
 */

"use strict";

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = function(context) {
    var options = context.options[0] || {};
    var minLength = typeof options.min !== "undefined" ? options.min : 2;
    var maxLength = typeof options.max !== "undefined" ? options.max : Infinity;
    var properties = options.properties !== "never";
    var exceptions = (options.exceptions ? options.exceptions : [])
        .reduce(function(obj, item) {
            obj[item] = true;

            return obj;
        }, {});

    var SUPPORTED_EXPRESSIONS = {
        "MemberExpression": properties && function(parent) {
            return !parent.computed && (

                // regular property assignment
                parent.parent.left === parent || (

                    // or the last identifier in an ObjectPattern destructuring
                    parent.parent.type === "Property" && parent.parent.value === parent &&
                    parent.parent.parent.type === "ObjectPattern" && parent.parent.parent.parent.left === parent.parent.parent
                )
            );
        },
        "AssignmentPattern": function(parent, node) {
            return parent.left === node;
        },
        "VariableDeclarator": function(parent, node) {
            return parent.id === node;
        },
        "Property": properties && function(parent, node) {
            return parent.key === node;
        },
        "ImportDefaultSpecifier": true,
        "RestElement": true,
        "FunctionExpression": true,
        "ArrowFunctionExpression": true,
        "ClassDeclaration": true,
        "FunctionDeclaration": true,
        "MethodDefinition": true,
        "CatchClause": true
    };

    return {
        Identifier: function(node) {
            var name = node.name;
            var parent = node.parent;

            var isShort = name.length < minLength;
            var isLong = name.length > maxLength;

            if (!(isShort || isLong) || exceptions[name]) {
                return;  // Nothing to report
            }

            var isValidExpression = SUPPORTED_EXPRESSIONS[parent.type];

            if (isValidExpression && (isValidExpression === true || isValidExpression(parent, node))) {
                context.report(
                    node,
                    isShort ?
                        "Identifier name '{{name}}' is too short. (< {{min}})" :
                        "Identifier name '{{name}}' is too long. (> {{max}})",
                    { name: name, min: minLength, max: maxLength }
                );
            }
        }
    };
};

module.exports.schema = [
    {
        "type": "object",
        "properties": {
            "min": {
                "type": "number"
            },
            "max": {
                "type": "number"
            },
            "exceptions": {
                "type": "array",
                "uniqueItems": true,
                "items": {
                    "type": "string"
                }
            },
            "properties": {
                "enum": ["always", "never"]
            }
        },
        "additionalProperties": false
    }
];
