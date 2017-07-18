/**
 * @fileoverview Default config options
 * @author Teddy Katz
 */

"use strict";

/**
 * Freezes an object and all its nested properties
 * @param {Object} obj The object to deeply freeze
 * @returns {Object} `obj` after freezing it
 */
function deepFreeze(obj) {
    if (obj === null || typeof obj !== "object") {
        return obj;
    }

    Object.keys(obj).map(key => obj[key]).forEach(deepFreeze);
    return Object.freeze(obj);
}

module.exports = deepFreeze({
    env: {},
    globals: {},
    rules: {},
    settings: {},
    parser: "espree",
    parserOptions: {
        ecmaVersion: 5,
        sourceType: "script",
        ecmaFeatures: {}
    }
});
