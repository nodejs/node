/**
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @module final-newline
 * @fileoverview
 *   Warn when a newline at the end of a file is missing.
 *
 *   See [StackExchange](http://unix.stackexchange.com/questions/18743) for
 *   why.
 */

'use strict';

/* Expose. */
module.exports = finalNewline;

/**
 * Warn when the list-item marker style of unordered lists
 * violate a given style.
 *
 * @param {Node} ast - Root node.
 * @param {File} file - Virtual file.
 */
function finalNewline(ast, file) {
  var contents = file.toString();
  var last = contents.length - 1;

  if (last > -1 && contents.charAt(last) !== '\n') {
    file.message('Missing newline character at end of file');
  }
}
