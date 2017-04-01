/**
 * @author Titus Wormer
 * @copyright 2015-2016 Titus Wormer
 * @license MIT
 * @module unified-engine:file-pipeline:read
 * @fileoverview Read a file if not already filled.
 */

'use strict';

/* Dependencies. */
var fs = require('fs');
var path = require('path');
var debug = require('debug')('unified-engine:file-pipeline:read');
var stats = require('vfile-statistics');

/* Expose. */
module.exports = read;

/* Methods. */
var resolve = path.resolve;
var readFile = fs.readFile;

/* Constants. */
var ENC = 'utf-8';

/**
 * Fill a file with its contents when not already filled.
 *
 * @param {Object} context - Context.
 * @param {File} file - File.
 * @param {FileSet} fileSet - Set.
 * @param {function(Error?)} done - Completion handler.
 */
function read(context, file, fileSet, done) {
  var filePath = file.path;

  if (file.contents || file.data.unifiedEngineStreamIn) {
    debug('Not reading file `%s` with contents', filePath);
    done();
  } else if (stats(file).fatal) {
    debug('Not reading failed file `%s`', filePath);
    done();
  } else {
    filePath = resolve(context.cwd, filePath);

    debug('Reading `%s` in `%s`', filePath, ENC);

    readFile(filePath, ENC, function (err, contents) {
      debug('Read `%s` (err: %s)', filePath, err);

      file.contents = contents || '';

      done(err);
    });
  }
}
