'use strict';

var debug = require('debug')('unified-engine:file-pipeline:configure');
var stats = require('vfile-statistics');
var fnName = require('fn-name');
var object = require('is-object');
var empty = require('is-empty');

module.exports = configure;

/* Collect configuration for a file based on the context. */
function configure(context, file, fileSet, next) {
  var config = context.configuration;
  var processor = context.processor;

  if (stats(file).fatal) {
    return next();
  }

  config.load(file.path, handleConfiguration);

  function handleConfiguration(err, configuration) {
    var plugins;
    var options;
    var plugin;
    var length;
    var index;
    var name;

    if (err) {
      return next(err);
    }

    /* Store configuration on the context object. */
    debug('Using settings `%j`', configuration.settings);
    processor.data('settings', configuration.settings);

    plugins = configuration.plugins;
    length = plugins.length;
    index = -1;

    debug('Using `%d` plugins', length);

    while (++index < length) {
      plugin = plugins[index][0];
      options = plugins[index][1];

      if (options === false) {
        continue;
      }

      /* Allow for default arguments in es2020. */
      if (options === null || (object(options) && empty(options))) {
        options = undefined;
      }

      name = fnName(plugin) || 'function';
      debug('Using plug-in `%s`, with options `%j`', name, options);

      try {
        processor.use(plugin, options, fileSet);
      } catch (err) {
        /* istanbul ignore next - Shouldn’t happen anymore! */
        return next(err);
      }
    }

    next();
  }
}
