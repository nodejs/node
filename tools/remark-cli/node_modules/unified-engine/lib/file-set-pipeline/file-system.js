'use strict';

var Ignore = require('../ignore');
var find = require('../finder');

module.exports = fileSystem;

/* Find files from the file-system. */
function fileSystem(context, settings, next) {
  var input = context.files;
  var skip = settings.silentlyIgnore;
  var ignore = new Ignore({
    cwd: settings.cwd,
    detectIgnore: settings.detectIgnore,
    ignoreName: settings.ignoreName,
    ignorePath: settings.ignorePath
  });

  if (input.length === 0) {
    return next();
  }

  find(input, {
    cwd: settings.cwd,
    extensions: settings.extensions,
    silentlyIgnore: skip,
    ignore: ignore
  }, found);

  function found(err, result) {
    var output = result.files;

    /* Sort alphabetically. Everything’s unique so we don’t care
     * about cases where left and right are equal. */
    output.sort(function (left, right) {
      return left.path < right.path ? -1 : 1;
    });

    /* Mark as given.  This allows outputting files,
     * which can be pretty dangerous, so it’s “hidden”. */
    output.forEach(function (file) {
      file.data.unifiedEngineGiven = true;
    });

    context.files = output;

    /* If `out` wasn’t set, detect it based on
     * whether one file was given. */
    if (settings.out === null || settings.out === undefined) {
      settings.out = result.oneFileMode;
    }

    next(err);
  }
}
