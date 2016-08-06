/**
 * @author Titus Wormer
 * @copyright 2015-2016 Titus Wormer
 * @license MIT
 * @module unified-engine:log
 * @fileoverview Log a file context on successful completion.
 */

'use strict';

/* Dependencies. */
var report = require('vfile-reporter');

/* Expose. */
module.exports = log;

/**
 * Output diagnostics to `streamError`.
 *
 * @param {Object} context - Context.
 * @param {Object} settings - Configuration.
 * @param {function(Error?)} next - Completion handler.
 */
function log(context, settings, next) {
  var diagnostics = report(context.files.filter(given), {
    quiet: settings.quiet,
    silent: settings.silent,
    color: settings.color
  });

  if (diagnostics) {
    settings.streamError.write(diagnostics + '\n', next);
  } else {
    next();
  }
}

/**
 * Whether was detected by `finder`.
 *
 * @param {VFile} file - Virtual file.
 * @return {boolean} - Whether given by user.
 */
function given(file) {
  return file.data.unifiedEngineGiven;
}
