'use strict';

var PassThrough = require('stream').PassThrough;
var statistics = require('vfile-statistics');
var fileSetPipeline = require('./file-set-pipeline');

module.exports = run;

/* Run the file set pipeline once.
 * `callback` is invoked with a fatal error,
 * or with a status code (`0` on success, `1` on failure). */
function run(options, callback) {
  var settings = {};
  var stdin = new PassThrough();
  var tree;
  var detectConfig;
  var hasConfig;
  var detectIgnore;
  var hasIgnore;

  try {
    stdin = process.stdin;
  } catch (err) {
    /* Obscure bug in Node (seen on windows):
     * - https://github.com/nodejs/node/blob/f856234/lib/internal/
     *   process/stdio.js#L82;
     * - https://github.com/AtomLinter/linter-markdown/pull/85.
     */
  }

  if (!callback) {
    throw new Error('Missing `callback`');
  }

  if (!options || !options.processor) {
    return next(new Error('Missing `processor`'));
  }

  /* Processor. */
  settings.processor = options.processor;

  /* Path to run as. */
  settings.cwd = options.cwd || process.cwd();

  /* Input. */
  settings.files = options.files || [];
  settings.extensions = (options.extensions || []).map(function (extension) {
    return extension.charAt(0) === '.' ? extension : '.' + extension;
  });

  settings.filePath = options.filePath || null;
  settings.streamIn = options.streamIn || stdin;

  /* Output. */
  settings.streamOut = options.streamOut || process.stdout;
  settings.streamError = options.streamError || process.stderr;
  settings.alwaysStringify = options.alwaysStringify;
  settings.output = options.output;
  settings.out = options.out;

  /* Null overwrites config settings, `undefined` doesn’t. */
  if (settings.output === null || settings.output === undefined) {
    settings.output = undefined;
  }

  if (settings.output && settings.out) {
    return next(new Error('Cannot accept both `output` and `out`'));
  }

  /* Process phase management. */
  tree = options.tree || false;

  settings.treeIn = options.treeIn;
  settings.treeOut = options.treeOut;

  if (settings.treeIn === null || settings.treeIn === undefined) {
    settings.treeIn = tree;
  }

  if (settings.treeOut === null || settings.treeOut === undefined) {
    settings.treeOut = tree;
  }

  /* Configuration. */
  detectConfig = options.detectConfig;
  hasConfig = Boolean(options.rcName || options.packageField);

  if (detectConfig && !hasConfig) {
    return next(new Error(
      'Missing `rcName` or `packageField` with `detectConfig`'
    ));
  }

  settings.detectConfig = detectConfig === null || detectConfig === undefined ? hasConfig : detectConfig;
  settings.rcName = options.rcName || null;
  settings.rcPath = options.rcPath || null;
  settings.packageField = options.packageField || null;
  settings.settings = options.settings || {};
  settings.configTransform = options.configTransform;
  settings.defaultConfig = options.defaultConfig;

  /* Ignore. */
  detectIgnore = options.detectIgnore;
  hasIgnore = Boolean(options.ignoreName);

  settings.detectIgnore = detectIgnore === null || detectIgnore === undefined ? hasIgnore : detectIgnore;
  settings.ignoreName = options.ignoreName || null;
  settings.ignorePath = options.ignorePath || null;
  settings.silentlyIgnore = Boolean(options.silentlyIgnore);

  if (detectIgnore && !hasIgnore) {
    return next(new Error('Missing `ignoreName` with `detectIgnore`'));
  }

  /* Plug-ins. */
  settings.pluginPrefix = options.pluginPrefix || null;
  settings.plugins = options.plugins || {};

  /* Reporting. */
  settings.color = options.color || false;
  settings.silent = options.silent || false;
  settings.quiet = options.quiet || false;
  settings.frail = options.frail || false;

  /* Process. */
  fileSetPipeline.run({files: options.files || []}, settings, next);

  function next(err, context) {
    var stats = statistics((context || {}).files);
    var failed = Boolean(settings.frail ? stats.total : stats.fatal);

    if (err) {
      callback(err);
    } else {
      callback(null, failed ? 1 : 0, context);
    }
  }
}
