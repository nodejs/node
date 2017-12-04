'use strict';

var debug = require('debug')('unified-engine:file-pipeline:transform');
var stats = require('vfile-statistics');

module.exports = transform;

/* Transform the tree associated with a file with
 * configured plug-ins. */
function transform(context, file, fileSet, next) {
  if (stats(file).fatal) {
    next();
    return;
  }

  debug('Transforming document `%s`', file.path);

  context.processor.run(context.tree, file, function (err, node) {
    debug('Transformed document (error: %s)', err);
    context.tree = node;
    next(err);
  });
}
