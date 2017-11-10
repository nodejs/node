'use strict';

var FileSet = require('../file-set');
var filePipeline = require('../file-pipeline');

module.exports = transform;

/* Transform all files. */
function transform(context, settings, next) {
  var fileSet = new FileSet();

  context.fileSet = fileSet;

  fileSet.on('add', add).on('done', next);

  context.files.forEach(fileSet.add, fileSet);

  function add(file) {
    filePipeline.run({
      configuration: context.configuration,
      processor: settings.processor(),
      cwd: settings.cwd,
      extensions: settings.extensions,
      pluginPrefix: settings.pluginPrefix,
      treeIn: settings.treeIn,
      treeOut: settings.treeOut,
      out: settings.out,
      output: settings.output,
      streamOut: settings.streamOut,
      alwaysStringify: settings.alwaysStringify
    }, file, fileSet, done);

    function done(err) {
      /* istanbul ignore next - doesn’t occur as all
       * failures in `filePipeLine` are failed on each
       * file.  Still, just to ensure things work in
       * the future, we add an extra check. */
      if (err) {
        err = file.message(err);
        err.fatal = true;
      }

      fileSet.emit('one', file);
    }
  }
}
