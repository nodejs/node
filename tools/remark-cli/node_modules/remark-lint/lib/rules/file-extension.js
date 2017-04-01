/**
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @module file-extension
 * @fileoverview
 *   Warn when the document’s extension differs from the given preferred
 *   extension.
 *
 *   Does not warn when given documents have no file extensions (such as
 *   `AUTHORS` or `LICENSE`).
 *
 *   Options: `string`, default: `'md'` — Expected file extension.
 *
 * @example {"name": "readme.md"}
 *
 * @example {"name": "readme"}
 *
 * @example {"name": "readme.mkd", "label": "output", "config": {"positionless": true}}
 *
 *   1:1: Invalid extension: use `md`
 *
 * @example {"name": "readme.mkd", "setting": "mkd"}
 */

'use strict';

/* Expose. */
module.exports = fileExtension;

/**
 * Check file extensions.
 *
 * @param {Node} ast - Root node.
 * @param {File} file - Virtual file.
 * @param {string?} [preferred='md'] - Expected file
 *   extension.
 */
function fileExtension(ast, file, preferred) {
  var ext = file.extname;

  if (ext) {
    ext = ext.slice(1);
  }

  preferred = typeof preferred === 'string' ? preferred : 'md';

  if (ext && ext !== preferred) {
    console.log('e: ', [ext, preferred, file.extname], file);
    file.message('Invalid extension: use `' + preferred + '`');
  }
}
