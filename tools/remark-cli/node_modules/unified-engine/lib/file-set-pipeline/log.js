'use strict';

var xtend = require('xtend');
var load = require('load-plugin');
var report = require('vfile-reporter');
var string = require('x-is-string');

module.exports = log;

var prefix = 'vfile-reporter';

function log(context, settings, next) {
  var reporter = settings.reporter || report;
  var diagnostics;

  if (string(reporter)) {
    try {
      reporter = load(reporter, {cwd: settings.cwd, prefix: prefix});
    } catch (err) {
      next(new Error('Could not find reporter `' + reporter + '`'));
      return;
    }
  }

  diagnostics = reporter(context.files.filter(given), xtend(settings.reporterOptions, {
    quiet: settings.quiet,
    silent: settings.silent,
    color: settings.color
  }));

  if (diagnostics) {
    if (diagnostics.charAt(diagnostics.length - 1) !== '\n') {
      diagnostics += '\n';
    }

    settings.streamError.write(diagnostics, next);
  } else {
    next();
  }
}

function given(file) {
  return file.data.unifiedEngineGiven;
}
