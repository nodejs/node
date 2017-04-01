/**
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @module stringify-entities
 * @fileoverview Encode HTML character references and character entities.
 */

'use strict';

/* eslint-env commonjs */

/* Dependencies. */
var entities = require('character-entities-html4');
var legacy = require('character-entities-legacy');
var dangerous = require('./lib/dangerous.json');

/* Methods. */
var has = {}.hasOwnProperty;

/* List of enforced escapes. */
var escapes = ['"', '\'', '<', '>', '&', '`'];

/* Map of characters to names. */
var characters = {};

(function () {
  var name;

  for (name in entities) {
    characters[entities[name]] = name;
  }
})();

/* Default escapes. */
var EXPRESSION_ESCAPE = toExpression(escapes);

/* Surrogate pairs. */
var EXPRESSION_SURROGATE_PAIR = /[\uD800-\uDBFF][\uDC00-\uDFFF]/g;

/* Non-ASCII characters. */
var EXPRESSION_BMP = /[\x01-\t\x0B\f\x0E-\x1F\x7F\x81\x8D\x8F\x90\x9D\xA0-\uFFFF]/g;

/**
 * Get the first character in `char`.
 *
 * @param {string} char - Value.
 * @return {string} - First character.
 */
function charCode(char) {
  return char.charCodeAt(0);
}

/**
 * Check whether `char` is an alphanumeric.
 *
 * @param {string} char - Value.
 * @return {boolean} - Whether `char` is an
 *   alphanumeric.
 */
function isAlphanumeric(char) {
  var code = charCode(char);

  return (code >= 48 /* 0 */ && code <= 57 /* 9 */) ||
    (code >= 65 /* A */ && code <= 90 /* Z */) ||
    (code >= 97 /* a */ && code <= 122 /* z */);
}

/**
 * Check whether `char` is a hexadecimal.
 *
 * @param {string} char - Value.
 * @return {boolean} - Whether `char` is a
 *   hexadecimal.
 */
function isHexadecimal(char) {
  var code = charCode(char);

  return (code >= 48 /* 0 */ && code <= 57 /* 9 */) ||
    (code >= 65 /* A */ && code <= 70 /* F */) ||
    (code >= 97 /* a */ && code <= 102 /* f */);
}

/**
 * Transform `code` into a hexadecimal character reference.
 *
 * @param {number} code - Number to encode.
 * @param {string?} [next] - Next character.
 * @param {boolean?} [omit] - Omit optional semi-colons.
 * @return {string} - `code` encoded as hexadecimal.
 */
function toHexReference(code, next, omit) {
  var value = '&#x' + code.toString(16).toUpperCase();

  return omit && next && !isHexadecimal(next) ? value : value + ';';
}

/**
 * Transform `code` into an entity.
 *
 * @param {string} name - Name to wrap.
 * @param {string?} [next] - Next character.
 * @param {boolean?} [omit] - Omit optional semi-colons.
 * @param {boolean?} [attribute] - Stringify as attribute.
 * @return {string} - `name` encoded as hexadecimal.
 */
function toNamed(name, next, omit, attribute) {
  var value = '&' + name;

  if (
    omit &&
    has.call(legacy, name) &&
    dangerous.indexOf(name) === -1 &&
    (!attribute || (next && next !== '=' && !isAlphanumeric(next)))
  ) {
    return value;
  }

  return value + ';';
}

/**
 * Create an expression for `characters`.
 *
 * @param {Array.<string>} characters - Characters.
 * @return {RegExp} - Expression.
 */
function toExpression(characters) {
  return new RegExp('[' + characters.join('') + ']', 'g');
}

/**
 * Encode `char` according to `options`.
 *
 * @param {string} char - Character to encode.
 * @param {string} next - Character following `char`.
 * @param {Object} options - Configuration.
 * @return {string} - Entity.
 */
function one(char, next, options) {
  var shortest = options.useShortestReferences;
  var omit = options.omitOptionalSemicolons;
  var named;
  var numeric;

  if (
    (shortest || options.useNamedReferences) &&
    has.call(characters, char)
  ) {
    named = toNamed(characters[char], next, omit, options.attribute);
  }

  if (shortest || !named) {
    numeric = toHexReference(charCode(char), next, omit);
  }

  if (named && (!shortest || named.length < numeric.length)) {
    return named;
  }

  return numeric;
}

/**
 * Encode special characters in `value`.
 *
 * @param {string} value - Value to encode.
 * @param {Object?} [options] - Configuration.
 * @param {boolean?} [options.escapeOnly=false]
 *   - Whether to only escape required characters.
 * @param {Array.<string>} [options.subset=[]]
 *   - Subset of characters to encode.
 * @param {boolean?} [options.useNamedReferences=false]
 *   - Whether to use entities where possible.
 * @param {boolean?} [options.omitOptionalSemicolons=false]
 *   - Whether to omit optional semi-colons.
 * @param {boolean?} [options.attribute=false]
 *   - Whether to stringifying and attribute.
 * @return {string} - Encoded `value`.
 */
function encode(value, options) {
  var settings = options || {};
  var subset = settings.subset;
  var set = subset ? toExpression(subset) : EXPRESSION_ESCAPE;
  var escapeOnly = settings.escapeOnly;
  var omit = settings.omitOptionalSemicolons;

  value = value.replace(set, function (char, pos, val) {
    return one(char, val.charAt(pos + 1), settings);
  });

  if (subset || escapeOnly) {
    return value;
  }

  return value
    .replace(EXPRESSION_SURROGATE_PAIR, function (pair, pos, val) {
      return toHexReference(
        ((pair.charCodeAt(0) - 0xD800) * 0x400) +
        pair.charCodeAt(1) - 0xDC00 + 0x10000,
        val.charAt(pos + 2),
        omit
      );
    })
    .replace(EXPRESSION_BMP, function (char, pos, val) {
      return one(char, val.charAt(pos + 1), settings);
    });
}

/**
 * Shortcut to escape special characters in HTML.
 *
 * @param {string} value - Value to encode.
 * @return {string} - Encoded `value`.
 */
function escape(value) {
  return encode(value, {
    escapeOnly: true,
    useNamedReferences: true
  });
}

encode.escape = escape;

/* Expose. */
module.exports = encode;
