/**
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @module no-file-name-irregular-characters
 * @fileoverview
 *   Warn when file names contain irregular characters: characters other
 *   than alpha-numericals, dashes, and dots (full-stops).
 *
 *   Options: `RegExp` or `string`, default: `'\\.a-zA-Z0-9-'`.
 *
 *   If a string is given, it will be wrapped in
 *   `new RegExp('[^' + preferred + ']')`.
 *
 *   Any match by the wrapped or given expressions triggers a
 *   warning.
 *
 * @example {"name": "plug-ins.md"}
 *
 * @example {"name": "plugins.md"}
 *
 * @example {"name": "plug_ins.md", "label": "output", "config": {"positionless": true}}
 *
 *   1:1: Do not use `_` in a file name
 *
 * @example {"name": "README.md", "label": "output", "setting": "\\.a-z0-9", "config": {"positionless": true}}
 *
 *   1:1: Do not use `R` in a file name
 *
 * @example {"name": "plug ins.md", "label": "output", "config": {"positionless": true}}
 *
 *   1:1: Do not use ` ` in a file name
 */

'use strict';

/* Expose. */
module.exports = noFileNameIrregularCharacters;

/**
 * Warn when file names contain characters other than
 * alpha-numericals, dashes, and dots (full-stops).
 *
 * @param {Node} ast - Root node.
 * @param {File} file - Virtual file.
 * @param {string|RegExp} preferred - RegExp, or string of
 *   characters (in which case it’s wrapped in
 *   `new RegExp('[^' + preferred + ']')`), that matches
 *   characters which should not be allowed.
 */
function noFileNameIrregularCharacters(ast, file, preferred) {
  var expression = preferred || /[^\\\.a-zA-Z0-9-]/;
  var match;

  if (typeof expression === 'string') {
    expression = new RegExp('[^' + expression + ']');
  }

  match = file.stem && file.stem.match(expression);

  if (match) {
    file.message('Do not use `' + match[0] + '` in a file name');
  }
}
