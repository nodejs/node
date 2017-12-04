'use strict';

var debug = require('debug')('unified-engine:file-pipeline:stdout');
var stats = require('vfile-statistics');

module.exports = stdout;

/* Write a virtual file to `streamOut`.
 * Ignored when `output` is given, more than one file
 * was processed, or `out` is false. */
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
