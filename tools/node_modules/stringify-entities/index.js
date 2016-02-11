/**
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @module stringify-entities
 * @fileoverview Encode HTML character references and character entities.
 */

'use strict';

/* eslint-env commonjs */

/*
 * Dependencies.
 */

var entities = require('character-entities-html4');
var EXPRESSION_NAMED = require('./lib/expression.js');

/*
 * Methods.
 */

var has = {}.hasOwnProperty;

/*
 * List of enforced escapes.
 */

var escapes = ['"', '\'', '<', '>', '&', '`'];

/*
 * Map of characters to names.
 */

var characters = {};

(function () {
    var name;

    for (name in entities) {
        characters[entities[name]] = name;
    }
})();

/*
 * Regular expressions.
 */

var EXPRESSION_ESCAPE = new RegExp('[' + escapes.join('') + ']', 'g');
var EXPRESSION_SURROGATE_PAIR = /[\uD800-\uDBFF][\uDC00-\uDFFF]/g;
var EXPRESSION_BMP = /[\x01-\t\x0B\f\x0E-\x1F\x7F\x81\x8D\x8F\x90\x9D\xA0-\uFFFF]/g;

/**
 * Transform `code` into a hexadecimal character reference.
 *
 * @param {number} code - Number to encode.
 * @return {string} - `code` encoded as hexadecimal.
 */
function characterCodeToHexadecimalReference(code) {
    return '&#x' + code.toString(16).toUpperCase() + ';';
}

/**
 * Transform `character` into a hexadecimal character
 * reference.
 *
 * @param {string} character - Character to encode.
 * @return {string} - `character` encoded as hexadecimal.
 */
function characterToHexadecimalReference(character) {
    return characterCodeToHexadecimalReference(character.charCodeAt(0));
}

/**
 * Transform `code` into an entity.
 *
 * @param {string} name - Name to wrap.
 * @return {string} - `name` encoded as hexadecimal.
 */
function toNamedEntity(name) {
    return '&' + name + ';';
}

/**
 * Transform `code` into an entity.
 *
 * @param {string} character - Character to encode.
 * @return {string} - `name` encoded as hexadecimal.
 */
function characterToNamedEntity(character) {
    return toNamedEntity(characters[character]);
}

/**
 * Encode special characters in `value`.
 *
 * @param {string} value - Value to encode.
 * @param {Object?} [options] - Configuration.
 * @param {boolean?} [options.escapeOnly=false]
 *   - Whether to only escape required characters.
 * @param {boolean?} [options.useNamedReferences=false]
 *   - Whether to use entities where possible.
 * @return {string} - Encoded `value`.
 */
function encode(value, options) {
    var settings = options || {};
    var escapeOnly = settings.escapeOnly;
    var named = settings.useNamedReferences;
    var map = named ? characters : null;

    value = value.replace(EXPRESSION_ESCAPE, function (character) {
        return map && has.call(map, character) ?
            toNamedEntity(map[character]) :
            characterToHexadecimalReference(character);
    });

    if (escapeOnly) {
        return value;
    }

    if (named) {
        value = value.replace(EXPRESSION_NAMED, characterToNamedEntity);
    }

    return value
        .replace(EXPRESSION_SURROGATE_PAIR, function (pair) {
            return characterCodeToHexadecimalReference(
                (pair.charCodeAt(0) - 0xD800) * 0x400 +
                pair.charCodeAt(1) - 0xDC00 + 0x10000
            );
        })
        .replace(EXPRESSION_BMP, characterToHexadecimalReference);
}

/**
 * Shortcut to escape special characters in HTML.
 *
 * @param {string} value - Value to encode.
 * @return {string} - Encoded `value`.
 */
function escape(value) {
    return encode(value, {
        'escapeOnly': true,
        'useNamedReferences': true
    });
}

encode.escape = escape;

/*
 * Expose.
 */

module.exports = encode;
