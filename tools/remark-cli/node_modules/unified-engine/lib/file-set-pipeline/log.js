'use strict';

var report = require('vfile-reporter');

module.exports = log;

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

function given(file) {
  return file.data.unifiedEngineGiven;
}
