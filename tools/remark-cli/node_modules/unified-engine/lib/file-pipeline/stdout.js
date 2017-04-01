/**
 * @author Titus Wormer
 * @copyright 2015-2016 Titus Wormer
 * @license MIT
 * @module unified-engine:file-pipeline:stdout
 * @fileoverview Write a file to `streamOut`.
 */

'use strict';

/* Dependencies. */
var debug = require('debug')('unified-engine:file-pipeline:stdout');
var stats = require('vfile-statistics');

/* Expose. */
module.exports = stdout;

/**
 * Write a virtual file to `streamOut`.
 * Ignored when `output` is given, more than one file
 * was processed, or `out` is false.
 *
 * @param {Object} context - Context.
 * @param {File} file - File.
 * @param {FileSet} fileSet - Set.
 * @param {function(Error?)} next - Completion handler.
 */
function stdout(context, file, fileSet, next) {
  if (!file.data.unifiedEngineGiven) {
    debug('Ignoring programmatically added file');
    next();
  } else if (stats(file).fatal || context.output || !context.out) {
    debug('Ignoring writing to `streamOut`');
    next();
  } else {
    debug('Writing document to `streamOut`');

    context.streamOut.write(file.toString(), next);
  }
}
