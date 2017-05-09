'use strict';

var entities = require('character-entities-html4');
var legacy = require('character-entities-legacy');
var has = require('has');
var hexadecimal = require('is-hexadecimal');
var alphanumerical = require('is-alphanumerical');
var dangerous = require('./dangerous.json');

/* Expose. */
module.exports = encode;

encode.escape = escape;

/* List of enforced escapes. */
var escapes = ['"', '\'', '<', '>', '&', '`'];

/* Map of characters to names. */
var characters = construct();

/* Default escapes. */
var EXPRESSION_ESCAPE = toExpression(escapes);

/* Surrogate pairs. */
var EXPRESSION_SURROGATE_PAIR = /[\uD800-\uDBFF][\uDC00-\uDFFF]/g;

/* Non-ASCII characters. */
// eslint-disable-next-line no-control-regex
var EXPRESSION_BMP = /[\x01-\t\x0B\f\x0E-\x1F\x7F\x81\x8D\x8F\x90\x9D\xA0-\uFFFF]/g;

/* Encode special characters in `value`. */
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

/* Shortcut to escape special characters in HTML. */
function escape(value) {
  return encode(value, {
    escapeOnly: true,
    useNamedReferences: true
  });
}

/* Encode `char` according to `options`. */
function one(char, next, options) {
  var shortest = options.useShortestReferences;
  var omit = options.omitOptionalSemicolons;
  var named;
  var numeric;

  if (
    (shortest || options.useNamedReferences) &&
    has(characters, char)
  ) {
    named = toNamed(characters[char], next, omit, options.attribute);
  }

  if (shortest || !named) {
    numeric = toHexReference(char.charCodeAt(0), next, omit);
  }

  if (named && (!shortest || named.length < numeric.length)) {
    return named;
  }

  return numeric;
}

/* Transform `code` into an entity. */
function toNamed(name, next, omit, attribute) {
  var value = '&' + name;

  if (
    omit &&
    has(legacy, name) &&
    dangerous.indexOf(name) === -1 &&
    (!attribute || (next && next !== '=' && !alphanumerical(next)))
  ) {
    return value;
  }

  return value + ';';
}

/* Transform `code` into a hexadecimal character reference. */
function toHexReference(code, next, omit) {
  var value = '&#x' + code.toString(16).toUpperCase();
  return omit && next && !hexadecimal(next) ? value : value + ';';
}

/* Create an expression for `characters`. */
function toExpression(characters) {
  return new RegExp('[' + characters.join('') + ']', 'g');
}

/* Construct the map. */
function construct() {
  var chars = {};
  var name;

  for (name in entities) {
    chars[entities[name]] = name;
  }

  return chars;
}
