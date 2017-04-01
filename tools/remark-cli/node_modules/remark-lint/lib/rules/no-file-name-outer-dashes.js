/**
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @module no-file-name-outer-dashes
 * @fileoverview
 *   Warn when file names contain initial or final dashes.
 *
 * @example {"name": "readme.md"}
 *
 * @example {"name": "-readme.md", "label": "output", "config": {"positionless": true}}
 *
 *   1:1: Do not use initial or final dashes in a file name
 *
 * @example {"name": "readme-.md", "label": "output", "config": {"positionless": true}}
 *
 *   1:1: Do not use initial or final dashes in a file name
 */

'use strict';

/* Expose. */
module.exports = noFileNameOuterDashes;

/**
 * Warn when file names contain initial or final dashes.
 *
 * @param {Node} ast - Root node.
 * @param {File} file - Virtual file.
 */
function noFileNameOuterDashes(ast, file) {
  if (file.stem && /^-|-$/.test(file.stem)) {
    file.message('Do not use initial or final dashes in a file name');
  }
}
