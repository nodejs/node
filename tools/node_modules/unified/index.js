/**
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @module unified
 * @fileoverview Parse / Transform / Compile / Repeat.
 */

'use strict';

/* eslint-env commonjs */

/*
 * Dependencies.
 */

var bail = require('bail');
var ware = require('ware');
var AttachWare = require('attach-ware')(ware);
var VFile = require('vfile');
var unherit = require('unherit');
var extend;

try {
    extend = require('node-extend');
} catch (e) {
    extend = require('extend');
}

/*
 * Processing pipeline.
 */

var pipeline = ware()
    .use(function (ctx) {
        ctx.tree = ctx.context.parse(ctx.file, ctx.settings);
    })
    .use(function (ctx, next) {
        ctx.context.run(ctx.tree, ctx.file, next);
    })
    .use(function (ctx) {
        ctx.result = ctx.context.stringify(ctx.tree, ctx.file, ctx.settings);
    });

/**
 * Construct a new Processor class based on the
 * given options.
 *
 * @param {Object} options - Configuration.
 * @param {string} options.name - Private storage.
 * @param {Function} options.Parser - Class to turn a
 *   virtual file into a syntax tree.
 * @param {Function} options.Compiler - Class to turn a
 *   syntax tree into a string.
 * @return {Processor} - A new constructor.
 */
function unified(options) {
    var name = options.name;
    var Parser = options.Parser;
    var Compiler = options.Compiler;
    var data = options.data;

    /**
     * Construct a Processor instance.
     *
     * @constructor
     * @class {Processor}
     */
    function Processor(processor) {
        var self = this;

        if (!(self instanceof Processor)) {
            return new Processor(processor);
        }

        self.ware = new AttachWare(processor && processor.ware);
        self.ware.context = self;

        self.Parser = unherit(Parser);
        self.Compiler = unherit(Compiler);

        if (self.data) {
            self.data = extend(true, {}, self.data);
        }
    }

    /**
     * Either return `context` if its an instance
     * of `Processor` or construct a new `Processor`
     * instance.
     *
     * @private
     * @param {Processor?} [context] - Context object.
     * @return {Processor} - Either `context` or a new
     *   Processor instance.
     */
    function instance(context) {
        return context instanceof Processor ? context : new Processor();
    }

    /**
     * Attach a plugin.
     *
     * @this {Processor?} - Either a Processor instance or
     *   the Processor constructor.
     * @return {Processor} - Either `context` or a new
     *   Processor instance.
     */
    function use() {
        var self = instance(this);

        self.ware.use.apply(self.ware, arguments);

        return self;
    }

    /**
     * Transform.
     *
     * @this {Processor?} - Either a Processor instance or
     *   the Processor constructor.
     * @param {Node} [node] - Syntax tree.
     * @param {VFile?} [file] - Virtual file.
     * @param {Function?} [done] - Callback.
     * @return {Node} - `node`.
     */
    function run(node, file, done) {
        var self = this;
        var space;

        if (typeof file === 'function') {
            done = file;
            file = null;
        }

        if (!file && node && !node.type) {
            file = node;
            node = null;
        }

        file = new VFile(file);
        space = file.namespace(name);

        if (!node) {
            node = space.tree || node;
        } else if (!space.tree) {
            space.tree = node;
        }

        if (!node) {
            throw new Error('Expected node, got ' + node);
        }

        done = typeof done === 'function' ? done : bail;

        /*
         * Only run when this is an instance of Processor,
         * and when there are transformers.
         */

        if (self.ware && self.ware.fns) {
            self.ware.run(node, file, done);
        } else {
            done(null, node, file);
        }

        return node;
    }

    /**
     * Parse a file.
     *
     * Patches the parsed node onto the `name`
     * namespace on the `type` property.
     *
     * @this {Processor?} - Either a Processor instance or
     *   the Processor constructor.
     * @param {string|VFile} value - Input to parse.
     * @param {Object?} [settings] - Configuration.
     * @return {Node} - `node`.
     */
    function parse(value, settings) {
        var file = new VFile(value);
        var CustomParser = (this && this.Parser) || Parser;
        var node = new CustomParser(file, settings, instance(this)).parse();

        file.namespace(name).tree = node;

        return node;
    }

    /**
     * Compile a file.
     *
     * Used the parsed node at the `name`
     * namespace at `'tree'` when no node was given.
     *
     * @this {Processor?} - Either a Processor instance or
     *   the Processor constructor.
     * @param {Object} [node] - Syntax tree.
     * @param {VFile} [file] - File with syntax tree.
     * @param {Object?} [settings] - Configuration.
     * @return {string} - Compiled `file`.
     */
    function stringify(node, file, settings) {
        var CustomCompiler = (this && this.Compiler) || Compiler;
        var space;

        if (settings === null || settings === undefined) {
            settings = file;
            file = null;
        }

        if (!file && node && !node.type) {
            file = node;
            node = null;
        }

        file = new VFile(file);
        space = file.namespace(name);

        if (!node) {
            node = space.tree || node;
        } else if (!space.tree) {
            space.tree = node;
        }

        if (!node) {
            throw new Error('Expected node, got ' + node);
        }

        return new CustomCompiler(file, settings, instance(this)).compile();
    }

    /**
     * Parse / Transform / Compile.
     *
     * @this {Processor?} - Either a Processor instance or
     *   the Processor constructor.
     * @param {string|VFile} value - Input to process.
     * @param {Object?} [settings] - Configuration.
     * @param {Function?} [done] - Callback.
     * @return {string?} - Parsed document, when
     *   transformation was async.
     */
    function process(value, settings, done) {
        var self = instance(this);
        var file = new VFile(value);
        var result = null;

        if (typeof settings === 'function') {
            done = settings;
            settings = null;
        }

        pipeline.run({
            'context': self,
            'file': file,
            'settings': settings || {}
        }, function (err, res) {
            result = res && res.result;

            if (done) {
                done(err, file, result);
            } else if (err) {
                bail(err);
            }
        });

        return result;
    }

    /*
     * Methods / functions.
     */

    var proto = Processor.prototype;

    Processor.use = proto.use = use;
    Processor.parse = proto.parse = parse;
    Processor.run = proto.run = run;
    Processor.stringify = proto.stringify = stringify;
    Processor.process = proto.process = process;
    Processor.data = proto.data = data || null;

    return Processor;
}

/*
 * Expose.
 */

module.exports = unified;
