'use strict';

var fs = require('fs');
var path = require('path');
var debug = require('debug')('unified-engine:file-pipeline:file-system');

module.exports = fileSystem;

var writeFile = fs.writeFile;
var resolve = path.resolve;

/* Write a virtual file to the file-system.
 * Ignored when `output` is not given. */
function fileSystem(context, file, fileSet, next) {
  var destinationPath;

  if (!context.output) {
    debug('Ignoring writing to file-system');
    return next();
  }

  if (!file.data.unifiedEngineGiven) {
    debug('Ignoring programmatically added file');
    return next();
  }

  destinationPath = file.path;

  if (!destinationPath) {
    debug('Ignoring file without output location');
    return next();
  }

  destinationPath = resolve(context.cwd, destinationPath);
  debug('Writing document to `%s`', destinationPath);

  file.stored = true;

  writeFile(destinationPath, file.toString(), next);
}
