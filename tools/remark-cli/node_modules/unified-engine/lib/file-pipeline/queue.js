/**
 * @author Titus Wormer
 * @copyright 2015-2016 Titus Wormer
 * @license MIT
 * @module unified-engine:file-pipeline:queue
 * @fileoverview Queue all files which came this far.
 */

'use strict';

/* Dependencies. */
var debug = require('debug')('unified-engine:file-pipeline:queue');
var stats = require('vfile-statistics');

/* Expose. */
module.exports = queue;

/**
 * Queue all files which came this far.
 * When the last file gets here, run the file-set pipeline
 * and flush the queue.
 *
 * @param {Object} context - Context.
 * @param {File} file - File.
 * @param {FileSet} fileSet - Set.
 * @param {function(Error?)} done - Completion handler.
 */
function queue(context, file, fileSet, done) {
  var origin = file.history[0];
  var map = fileSet.complete;
  var complete = true;

  if (!map) {
    map = fileSet.complete = {};
  }

  debug('Queueing `%s`', origin);

  map[origin] = done;

  fileSet.valueOf().forEach(function (file) {
    var key = file.history[0];

    if (stats(file).fatal) {
      return;
    }

    if (typeof map[key] === 'function') {
      debug('`%s` can be flushed', key);
    } else {
      debug('Interupting flush: `%s` is not finished', key);
      complete = false;
    }
  });

  if (!complete) {
    debug('Not flushing: some files cannot be flushed');
    return;
  }

  fileSet.complete = {};

  fileSet.pipeline.run(fileSet, function (err) {
    debug('Flushing: all files can be flushed');

    /* Flush. */
    for (origin in map) {
      map[origin](err);
    }
  });
}
