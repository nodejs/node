/**
 * @fileoverview Rule to flag use of constructors without capital letters
 * @author Nicholas C. Zakas
 * @copyright 2014 Jordan Harband. All rights reserved.
 * @copyright 2013-2014 Nicholas C. Zakas. All rights reserved.
 */

"use strict";

//------------------------------------------------------------------------------
// Requirements
//------------------------------------------------------------------------------

var assign = require("object-assign");

//------------------------------------------------------------------------------
// Helpers
//------------------------------------------------------------------------------

var CAPS_ALLOWED = [
    "Array",
    "Boolean",
    "Date",
    "Error",
    "Function",
    "Number",
    "Object",
    "RegExp",
    "String",
    "Symbol"
];

/**
 * Ensure that if the key is provided, it must be an array.
 * @param {Object} obj Object to check with `key`.
 * @param {string} key Object key to check on `obj`.
 * @param {*} fallback If obj[key] is not present, this will be returned.
 * @returns {string[]} Returns obj[key] if it's an Array, otherwise `fallback`
 */
function checkArray(obj, key, fallback) {
    /* istanbul ignore if */
    if (Object.prototype.hasOwnProperty.call(obj, key) && !Array.isArray(obj[key])) {
        throw new TypeError(key + ", if provided, must be an Array");
    }
    return obj[key] || fallback;
}

/**
 * A reducer function to invert an array to an Object mapping the string form of the key, to `true`.
 * @param {Object} map Accumulator object for the reduce.
 * @param {string} key Object key to set to `true`.
 * @returns {Object} Returns the updated Object for further reduction.
 */
function invert(map, key) {
    map[key] = true;
    return map;
}

/**
 * Creates an object with the cap is new exceptions as its keys and true as their values.
 * @param {Object} config Rule configuration
 * @returns {Object} Object with cap is new exceptions.
 */
function calculateCapIsNewExceptions(config) {
    var capIsNewExceptions = checkArray(config, "capIsNewExceptions", CAPS_ALLOWED);

    if (capIsNewExceptions !== CAPS_ALLOWED) {
        capIsNewExceptions = capIsNewExceptions.concat(CAPS_ALLOWED);
    }

    return capIsNewExceptions.reduce(invert, {});
}

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = function(context) {

    var config = context.options[0] ? assign({}, context.options[0]) : {};
    config.newIsCap = config.newIsCap !== false;
    config.capIsNew = config.capIsNew !== false;
    var skipProperties = config.properties === false;

    var newIsCapExceptions = checkArray(config, "newIsCapExceptions", []).reduce(invert, {});

    var capIsNewExceptions = calculateCapIsNewExceptions(config);

    var listeners = {};

    //--------------------------------------------------------------------------
    // Helpers
    //--------------------------------------------------------------------------

    /**
     * Get exact callee name from expression
     * @param {ASTNode} node CallExpression or NewExpression node
     * @returns {string} name
     */
    function extractNameFromExpression(node) {

        var name = "",
            property;

        if (node.callee.type === "MemberExpression") {
            property = node.callee.property;

            if (property.type === "Literal" && (typeof property.value === "string")) {
                name = property.value;
            } else if (property.type === "Identifier" && !node.callee.computed) {
                name = property.name;
            }
        } else {
            name = node.callee.name;
        }
        return name;
    }

    /**
     * Returns the capitalization state of the string -
     * Whether the first character is uppercase, lowercase, or non-alphabetic
     * @param {string} str String
     * @returns {string} capitalization state: "non-alpha", "lower", or "upper"
     */
    function getCap(str) {
        var firstChar = str.charAt(0);

        var firstCharLower = firstChar.toLowerCase();
        var firstCharUpper = firstChar.toUpperCase();

        if (firstCharLower === firstCharUpper) {
            // char has no uppercase variant, so it's non-alphabetic
            return "non-alpha";
        } else if (firstChar === firstCharLower) {
            return "lower";
        } else {
            return "upper";
        }
    }

    /**
     * Check if capitalization is allowed for a CallExpression
     * @param {Object} allowedMap Object mapping calleeName to a Boolean
     * @param {ASTNode} node CallExpression node
     * @param {string} calleeName Capitalized callee name from a CallExpression
     * @returns {Boolean} Returns true if the callee may be capitalized
     */
    function isCapAllowed(allowedMap, node, calleeName) {
        if (allowedMap[calleeName] || allowedMap[context.getSource(node.callee)]) {
            return true;
        }

        if (calleeName === "UTC" && node.callee.type === "MemberExpression") {
            // allow if callee is Date.UTC
            return node.callee.object.type === "Identifier" &&
                node.callee.object.name === "Date";
        }

        return skipProperties && node.callee.type === "MemberExpression";
    }

    /**
     * Reports the given message for the given node. The location will be the start of the property or the callee.
     * @param {ASTNode} node CallExpression or NewExpression node.
     * @param {string} message The message to report.
     * @returns {void}
     */
    function report(node, message) {
        var callee = node.callee;

        if (callee.type === "MemberExpression") {
            callee = callee.property;
        }

        context.report(node, callee.loc.start, message);
    }

    //--------------------------------------------------------------------------
    // Public
    //--------------------------------------------------------------------------

    if (config.newIsCap) {
        listeners.NewExpression = function(node) {

            var constructorName = extractNameFromExpression(node);
            if (constructorName) {
                var capitalization = getCap(constructorName);
                var isAllowed = capitalization !== "lower" || isCapAllowed(newIsCapExceptions, node, constructorName);
                if (!isAllowed) {
                    report(node, "A constructor name should not start with a lowercase letter.");
                }
            }
        };
    }

    if (config.capIsNew) {
        listeners.CallExpression = function(node) {

            var calleeName = extractNameFromExpression(node);
            if (calleeName) {
                var capitalization = getCap(calleeName);
                var isAllowed = capitalization !== "upper" || isCapAllowed(capIsNewExceptions, node, calleeName);
                if (!isAllowed) {
                    report(node, "A function with a name starting with an uppercase letter should only be used as a constructor.");
                }
            }
        };
    }

    return listeners;
};

module.exports.schema = [
    {
        "type": "object",
        "properties": {
            "newIsCap": {
                "type": "boolean"
            },
            "capIsNew": {
                "type": "boolean"
            },
            "newIsCapExceptions": {
                "type": "array",
                "items": {
                    "type": "string"
                }
            },
            "capIsNewExceptions": {
                "type": "array",
                "items": {
                    "type": "string"
                }
            },
            "properties": {
                "type": "boolean"
            }
        },
        "additionalProperties": false
    }
];
