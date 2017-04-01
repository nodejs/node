/**
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @module to-vfile
 * @fileoverview Create virtual files by file-path.
 */

'use strict';

/* Dependencies. */
var buffer = require('is-buffer');
var string = require('x-is-string');
var vfile = require('vfile');

/* Expose. */
module.exports = toVFile;

/**
 * Create a virtual file from a description.
 * If `options` is a string or a buffer, it’s used as the
 * path.  In all other cases, the options are passed through
 * to `vfile()`.
 *
 * @param {string|buffer|Object} description - File description.
 * @param {Object|string} [options] - `fs.readFile` options.
 */
function toVFile(options) {
  if (string(options) || buffer(options)) {
    options = {path: String(options)};
  }

  return vfile(options);
}
