/**
 * @author Titus Wormer
 * @copyright 2015-2016 Titus Wormer
 * @license MIT
 * @module unified-engine:file-set
 * @fileoverview Collection of virtual files.
 */

'use strict';

/* Dependencies. */
var events = require('events');
var inherits = require('util').inherits;
var trough = require('trough');
var toVFile = require('to-vfile');

/* Expose. */
module.exports = FileSet;

/**
 * Construct a new file-set.
 *
 * @constructor
 * @class {FileSet}
 */
function FileSet() {
  var self = this;

  self.files = [];
  self.origins = [];

  self.expected = 0;
  self.actual = 0;

  self.pipeline = trough();
  self.plugins = [];

  events.init.call(self);

  self.on('one', one.bind(self));
}

/* Events. */
inherits(FileSet, events.EventEmitter);

/* Expose methods. */
FileSet.prototype.valueOf = valueOf;
FileSet.prototype.use = use;
FileSet.prototype.add = add;

/**
 * Create an array representation of `fileSet`.
 *
 * @this {FileSet}
 * @return {Array.<File>} - Value at the `files` property
 *   in context.
 */
function valueOf() {
  return this.files;
}

/**
 * Attach middleware to the pipeline on `fileSet`.
 *
 * A plug-in (function) can have an `pluginId` property,
 * which is used to ignore duplicate attachment.
 *
 * This pipeline will later be run when when all attached
 * files are after the transforming stage.
 *
 * @this {FileSet}
 * @param {Function} plugin - Middleware.
 * @return {FileSet} - `this`; context object.
 */
function use(plugin) {
  var self = this;
  var pipeline = self.pipeline;
  var duplicate = false;

  if (plugin && plugin.pluginId) {
    duplicate = self.plugins.some(function (fn) {
      return fn.pluginId === plugin.pluginId;
    });
  }

  if (!duplicate && self.plugins.indexOf(plugin) !== -1) {
    duplicate = true;
  }

  if (!duplicate) {
    self.plugins.push(plugin);
    pipeline.use(plugin);
  }

  return this;
}

/**
 * Add a file to be processed.
 *
 * Ignores duplicate files (based on the `filePath` at time
 * of addition).
 *
 * Only runs `file-pipeline` on files which have not
 * `failed` before addition.
 *
 * @this {FileSet}
 * @param {File|string} file - Virtual file, or path.
 * @return {FileSet} - `this`; context object.
 */
function add(file) {
  var self = this;
  var origin;

  if (typeof file === 'string') {
    file = toVFile(file);
  }

  /* Prevent files from being added multiple times. */
  origin = file.history[0];

  if (self.origins.indexOf(origin) !== -1) {
    return self;
  }

  self.origins.push(origin);

  /* Add. */
  self.valueOf().push(file);
  self.expected++;

  /*
   * Force an asynchronous operation.
   * This ensures that files which fall through
   * the file pipeline quicker that expected (e.g.,
   * when already fatally failed) still queue up
   * correctly.
   */

  setImmediate(function () {
    self.emit('add', file);
  });

  return self;
}

/**
 * Utility invoked when a single file has completed it's
 * pipeline, triggering `done` when all files are complete.
 */
function one() {
  var self = this;

  self.actual++;

  if (self.actual >= self.expected) {
    self.emit('done');
  }
}
