/**
 * @author Titus Wormer
 * @copyright 2016 Titus Wormer
 * @license MIT
 * @module markdown-escapes
 * @fileoverview List of escapable characters in markdown.
 */

'use strict';

/* eslint-env commonjs */

/* Expose. */
module.exports = escapes;

/* Characters. */
var defaults = [
  '\\',
  '`',
  '*',
  '{',
  '}',
  '[',
  ']',
  '(',
  ')',
  '#',
  '+',
  '-',
  '.',
  '!',
  '_',
  '>'
];

var gfm = defaults.concat(['~', '|']);

var commonmark = gfm.concat([
  '\n',
  '"',
  '$',
  '%',
  '&',
  '\'',
  ',',
  '/',
  ':',
  ';',
  '<',
  '=',
  '?',
  '@',
  '^'
]);

/* Expose characters. */
escapes.default = defaults;
escapes.gfm = gfm;
escapes.commonmark = commonmark;

/**
 * Get markdown escapes.
 *
 * @param {Object?} [options] - Configuration.
 * @return {Array.<string>} - Escapes.
 */
function escapes(options) {
  var settings = options || {};

  if (settings.commonmark) {
    return commonmark;
  }

  return settings.gfm ? gfm : defaults;
}
