'use strict';

var debug = require('debug')('unified-engine:file-set-pipeline:stdin');
var vfile = require('to-vfile');
var concat = require('concat-stream');

module.exports = stdin;

function stdin(context, settings, next) {
  var streamIn = settings.streamIn;
  var err;

  if (context.files.length !== 0) {
    debug('Ignoring `streamIn`');

    if (settings.filePath) {
      err = new Error(
        'Do not pass both `--file-path` and real files.\n' +
        'Did you mean to pass stdin instead of files?'
      );
    }

    next(err);

    return;
  }

  if (streamIn.isTTY) {
    debug('Cannot read from `tty` stream');
    next(new Error('No input'));

    return;
  }

  debug('Reading from `streamIn`');

  streamIn.pipe(concat({encoding: 'string'}, read));

  function read(value) {
    var file = vfile(settings.filePath || undefined);

    debug('Read from `streamIn`');

    file.cwd = settings.cwd;
    file.contents = value;
    file.data.unifiedEngineGiven = true;
    file.data.unifiedEngineStreamIn = true;

    context.files = [file];

    /* If `out` wasn’t set, set `out`. */
    settings.out = settings.out === null || settings.out === undefined ? true : settings.out;

    next();
  }
}
