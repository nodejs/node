/**
 * @author Titus Wormer
 * @copyright 2015-2016 Titus Wormer
 * @license MIT
 * @module unified-engine:file-pipeline:copy
 * @fileoverview Move a file.
 */

'use strict';

/* Dependencies. */
var fs = require('fs');
var path = require('path');
var debug = require('debug')('unified-engine:file-pipeline:copy');

/* Expose. */
module.exports = copy;

/* Methods. */
var stat = fs.statSync;
var dirname = path.dirname;
var resolve = path.resolve;
var relative = path.relative;

/* Constants. */
var SEPERATOR = path.sep;

/**
 * Move a file.
 *
 * @param {Object} context - Context.
 * @param {File} file - File.
 * @param {FileSet} fileSet - Set.
 */
function copy(context, file, fileSet) {
  var output = context.output;
  var multi = fileSet.expected > 1;
  var outpath = output;
  var currentPath = file.path;
  var isDir;

  if (typeof outpath !== 'string') {
    debug('Not copying');

    return;
  }

  outpath = resolve(context.cwd, outpath);

  debug('Copying `%s`', currentPath);

  try {
    isDir = stat(outpath).isDirectory();
  } catch (err) {
    if (
      err.code !== 'ENOENT' ||
      output.charAt(output.length - 1) === SEPERATOR
    ) {
      err.message = 'Cannot read output directory. Error:\n' + err.message;

      throw err;
    }

    /* This throws, or the parent exists, which
     * is a directory, but we should keep the
     * basename of the given file. */
    stat(resolve(dirname(outpath))).isDirectory();
    isDir = false;
  }

  if (!isDir && multi) {
    throw new Error(
      'Cannot write multiple files to single output: ' + outpath
    );
  }

  outpath = relative(file.cwd, outpath);

  if (isDir) {
    file.dirname = outpath;
  } else {
    file.path = outpath;
  }

  debug('Copying document from %s to %s', currentPath, file.path);
}
