'use strict';

var debug = require('debug')('unified-engine:file-pipeline:stringify');
var stats = require('vfile-statistics');

module.exports = stringify;

/* Stringify a tree. */
function stringify(context, file) {
  var processor = context.processor;
  var tree = context.tree;
  var value;

  if (stats(file).fatal) {
    debug('Not compiling failed document');
    return;
  }

  if (!context.output && !context.out && !context.alwaysStringify) {
    debug('Not compiling document without output settings');
    return;
  }

  debug('Compiling `%s`', file.path);

  if (context.treeOut) {
    /* Add a `json` extension to ensure the file is correctly seen as JSON.
     * Only add it if there’s a path — not if the file is for example stdin. */
    if (file.path) {
      file.extname = '.json';
    }

    /* Add the line break to create a valid UNIX file. */
    value = JSON.stringify(tree, null, 2) + '\n';
  } else {
    value = processor.stringify(tree, file);
  }

  file.contents = value;

  debug('Compiled document');
}
