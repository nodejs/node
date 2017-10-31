'use strict';

var fs = require('fs');
var path = require('path');
var debug = require('debug')('unified-engine:file-pipeline:read');
var stats = require('vfile-statistics');

module.exports = read;

var resolve = path.resolve;
var readFile = fs.readFile;

/* Fill a file with its contents when not already filled. */
function read(context, file, fileSet, next) {
  var filePath = file.path;

  if (file.contents || file.data.unifiedEngineStreamIn) {
    debug('Not reading file `%s` with contents', filePath);
    next();
  } else if (stats(file).fatal) {
    debug('Not reading failed file `%s`', filePath);
    next();
  } else {
    filePath = resolve(context.cwd, filePath);

    debug('Reading `%s` in `%s`', filePath, 'utf8');

    readFile(filePath, 'utf8', function (err, contents) {
      debug('Read `%s` (err: %s)', filePath, err);

      file.contents = contents || '';

      next(err);
    });
  }
}
