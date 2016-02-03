/**
 * @author Titus Wormer
 * @copyright 2015-2016 Titus Wormer
 * @license MIT
 * @module remark:cli:file-set
 * @version 3.2.2
 * @fileoverview Collection of virtual files.
 */

'use strict';

/* eslint-env node */

/*
 * Dependencies.
 */

var ware = require('ware');
var toVFile = require('to-vfile');
var filePipeline = require('./file-pipeline');

/**
 * Locked `VFile#move`.
 *
 * @this {VFile}
 * @memberof {VFile}
 * @return {VFile} - Context object.
 */
function locked() {
    return this;
}

/**
 * Utility invoked when a single file has completed it's
 * pipeline, invoking `fileSet.done` when all files are
 * done.
 *
 * @example
 *   var fileSet = new FileSet(cli);
 *   fileSet.done = function () {console.log('done!');}
 *
 *   fileSet.add(new File())
 *   fileSet.add(new File())
 *
 *   one(fileSet);
 *   one(fileSet);
 *   // 'done!'
 *
 * @param {FileSet} fileSet - Set in which a file
 *   completed.
 */
function one(fileSet) {
    fileSet.count++;

    if (fileSet.count >= fileSet.length && fileSet.done) {
        fileSet.done();
        fileSet.done = null;
    }
}

/**
 * Construct a new file-set.
 *
 * @example
 *   var fileSet = new FileSet(cli);
 *
 * @constructor
 * @class {FileSet}
 * @param {CLI|Object} cli - As returned by `lib/cli/cli`.
 */
function FileSet(cli) {
    var self = this;

    /**
     * Files in the set.
     *
     * @member {Array.<VFile>} contents
     */
    self.contents = [];

    /**
     * Number of files in the set.
     *
     * @member {number} length
     */
    self.length = 0;

    /**
     * Number of processed files.
     *
     * @member {number} count
     */
    self.count = 0;

    /**
     * File-paths to the original location of files in
     * the set.
     *
     * @member {Array.<string>} soucePaths
     */
    self.sourcePaths = [];

    /**
     * CLI executing the set.
     *
     * @member {CLI} cli
     */
    self.cli = cli;

    /**
     * Pipeline to run when all files in the file-set
     * are processed.
     *
     * @member {Ware} pipeline
     */
    self.pipeline = ware();
}

/**
 * Create an array representation of `fileSet`.
 *
 * @example
 *   var fileSet = new FileSet(cli);
 *   fileSet.valueOf() // []
 *   fileSet.toJSON() // []
 *
 * @this {FileSet}
 * @return {Array.<File>} - Value at the `contents` property
 *   in context.
 */
function valueOf() {
    return this.contents;
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
 * @example
 *   var fileSet = new FileSet(cli);
 *   fileSet.use(console.log);
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
        duplicate = pipeline.fns.some(function (fn) {
            return fn.pluginId === plugin.pluginId;
        });
    }

    if (!duplicate && pipeline.fns.indexOf(plugin) !== -1) {
        duplicate = true;
    }

    if (!duplicate) {
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
 * @example
 *   var fileSet = new FileSet(cli);
 *   var fileA = new File({
 *     'directory': '~',
 *     'filename': 'example',
 *     'extension': 'md'
 *   });
 *   var fileB = new File({
 *     'directory': '~',
 *     'filename': 'example',
 *     'extension': 'md'
 *   });
 *
 *   fileSet.add(fileA);
 *   fileSet.length; // 1
 *
 *   fileSet.add(fileB);
 *   fileSet.length; // 1
 *
 * @this {FileSet}
 * @param {File|string} file - Virtual file, or path.
 * @return {FileSet} - `this`; context object.
 */
function add(file) {
    var self = this;
    var paths = self.sourcePaths;
    var sourcePath;
    var context;

    if (typeof file === 'string') {
        file = toVFile(file);
    }

    sourcePath = file.filePath();

    if (paths.indexOf(sourcePath) !== -1) {
        return self;
    }

    paths.push(sourcePath);

    file.sourcePath = sourcePath;

    if (!file.namespace('remark:cli').given) {
        file.move = locked;
    }

    self.length++;

    self.valueOf().push(file);

    context = {
        'file': file,
        'fileSet': self
    };

    /*
     * Force an asynchronous operation.
     * This ensures that files which fall through
     * the file pipeline quicker that expected (e.g.,
     * when already fatally failed) still queue up
     * correctly.
     */

    setImmediate(function () {
        filePipeline.run(context, function (err) {
            if (err) {
                file.fail(err);
            }

            one(self);
        });
    });

    return self;
}

/*
 * Expose methods.
 */

FileSet.prototype.valueOf = valueOf;
FileSet.prototype.toJSON = valueOf;
FileSet.prototype.use = use;
FileSet.prototype.add = add;

/*
 * Expose.
 */

module.exports = FileSet;
