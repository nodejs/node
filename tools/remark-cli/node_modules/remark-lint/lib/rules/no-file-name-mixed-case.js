/**
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @module no-file-name-mixed-case
 * @fileoverview
 *   Warn when a file name uses mixed case: both upper- and lower case
 *   characters.
 *
 * @example {"name": "README.md"}
 *
 * @example {"name": "readme.md"}
 *
 * @example {"name": "Readme.md", "label": "output", "config": {"positionless": true}}
 *
 *   1:1: Do not mix casing in file names
 */

'use strict';

/* Expose. */
module.exports = noFileNameMixedCase;

/**
 * Warn when a file name uses mixed case: both upper- and
 * lower case characters.
 *
 * @param {Node} ast - Root node.
 * @param {File} file - Virtual file.
 */
function noFileNameMixedCase(ast, file) {
  var name = file.stem;

  if (name && !(name === name.toLowerCase() || name === name.toUpperCase())) {
    file.message('Do not mix casing in file names');
  }
}
