/**
 * @author Titus Wormer
 * @copyright 2015-2016 Titus Wormer
 * @license MIT
 * @module remark:cli:cli
 * @version 3.2.2
 * @fileoverview Bridge between CLI options and node.
 */

'use strict';

/* eslint-env node */

/* eslint-disable no-console */

/*
 * Dependencies.
 */

var util = require('util');
var events = require('events');
var commander = require('commander');
var camelcase = require('camelcase');
var pack = require('../../package.json');
var Cache = require('./watch-output-cache');
var Spinner = require('./spinner');

/*
 * Methods.
 */

var Emitter = events.EventEmitter;
var Command = commander.Command;

/*
 * Constants.
 */

var SPLITTER = / *[,;] */g;
var EXTENSIONS = ['md', 'markdown', 'mkd', 'mkdn', 'mkdown', 'ron'];

/*
 * Set-up.
 */

var COMMAND = Object.keys(pack.bin)[0];

/**
 * Parse a (lazy?) JSON config.
 *
 * @example
 *   parseJSON('foo:true') // {'foo': true}
 *
 *   parseJSON('foo-bar: 1, baz-qux: -1') // {'foo-bar': 1, baz-qux: -1}
 *
 *   parseJSON('foo: ["bar", "baz"]') // {'foo': ['bar', 'baz']}
 *
 * @param {string} value - Value to parse as JSON.
 * @return {Object} - Parsed `value`.
 * @throws {Error} - When the augmented options could not
 *   be parsed.
 */
function parseJSON(value) {
    /*
     * Quote unquoted property keys.
     */

    value = value.replace(/([a-zA-Z0-9\-\/]+)(?=\s*:)/g, '\"$&\"');

    return JSON.parse('{' + value + '}');
}

/**
 * Transform the keys on an object to camel-case,
 * recursivly.
 *
 * @example
 *   toCamelCase({"foo-bar": {"qux-quux": "baz"}});
 *   // {"fooBar": {"quxQuux": "baz"}}
 *
 * @param {Object} object - Object to transform.
 * @return {Object} - New object, with camel-case keys.
 */
function toCamelCase(object) {
    var result = {};
    var value;
    var key;

    for (key in object) {
        value = object[key];

        if (value && typeof value === 'object' && !('length' in value)) {
            value = toCamelCase(value);
        }

        result[camelcase(key)] = value;
    }

    return result;
}

/**
 * Parse settings into an object.
 *
 * @example
 *   var cache = {};
 *   options('commonmark: true', cache);
 *
 * @param {string} flags - Command line settings.  These
 *   are just JSON, although the keys do not need double
 *   quotes to work.
 * @param {Object} cache - Settings store.
 * @return {Object} - `cache`.
 */
function options(flags, cache) {
    var flag;

    try {
        flags = toCamelCase(parseJSON(flags));
    } catch (exception) {
        exception.message = 'Cannot parse CLI settings: \nError: ' +
            exception.message;

        throw exception;
    }

    for (flag in flags) {
        cache[flag] = flags[flag];
    }

    return cache;
}

/**
 * Parse a plugin into its name and options.
 *
 * @example
 *   var plugin = parsePlugin('remark-toc=heading:"foo"');
 *
 * @param {string} plugin - Plugin name with options.
 * @return {Object} - Map with a `name`, referring to
 *   the parser name, and a `value`, which when available,
 *   contains the plug-in options.
 */
function parsePlugin(plugin) {
    var index = plugin.indexOf('=');
    var name;
    var value;

    if (index === -1) {
        name = plugin;
        value = null;
    } else {
        name = plugin.slice(0, index);
        value = options(plugin.slice(index + 1), {});
    }

    return {
        'name': name,
        'value': value
    };
}

/**
 * Parse plugins into a list.
 *
 * @example
 *   var cache = {};
 *   plugins('foo=bar:false', cache);
 *
 * @param {string} plugin - Plugin name with options.
 * @param {Object} cache - Plugin store.
 * @return {Object} - `cache`.
 */
function plugins(plugin, cache) {
    plugin = parsePlugin(plugin);

    cache[plugin.name] = plugin.value;

    return cache;
}

/**
 * Parse extensions into a list.
 *
 * @example
 *   var res = extensions('markdown,ron', ['md']);
 *
 * @param {string} extension - List of extensions.
 * @param {Array.<string>} cache - Extension store.
 * @return {Array.<string>} - New extensions store.
 */
function extensions(extension, cache) {
    return cache.concat(extension.split(SPLITTER));
}

/**
 * Help.
 */
function help() {
    console.log([
        '  See also: man 1 remark, man 3 remark,',
        '    man 3 remarkplugin, man 5 remarkrc,',
        '    man 5 remarkignore, man 7 remarksetting,',
        '    man 7 remarkconfig, man 7 remarkplugin.',
        '',
        '  Examples:',
        '',
        '    # Process `readme.md`',
        '    $ ' + COMMAND + ' readme.md -o readme-new.md',
        '',
        '    # Pass stdin(4) through remark, with settings, to stdout(4)',
        '    $ ' + COMMAND + ' --setting "setext: true, bullet: \\"*\\""' +
            ' < readme.md > readme-new.md',
        '',
        '    # Use a plugin (with options)',
        '    $ npm install remark-toc',
        '    $ ' + COMMAND + ' readme.md --use "toc=heading:\\"contents\\""' +
            ' -o',
        '',
        '    # Rewrite markdown in a directory',
        '    $ ' + COMMAND + ' . -o'
    ].join('\n'));
}

/*
 * Module.
 */

var program = new Command(pack.name)
    .version(pack.version)
    .description(pack.description)
    .usage('[options] <pathspec...>')
    .option('-o, --output [path]', 'specify output location')
    .option('-c, --config-path <path>', 'specify configuration location')
    .option('-i, --ignore-path <path>', 'specify ignore location')
    .option('-s, --setting <settings>', 'specify settings', options, {})
    .option('-u, --use <plugins>', 'use transform plugin(s)', plugins, {})
    .option('-e, --ext <extensions>', 'specify extensions', extensions, [])
    .option('-w, --watch', 'watch for changes and reprocess', false)
    .option('-a, --ast', 'output AST information', false)
    .option('-q, --quiet', 'output only warnings and errors', false)
    .option('-S, --silent', 'output only errors', false)
    .option('-f, --frail', 'exit with 1 on warnings', false)
    .option('--file-path <path>', 'specify file path to process as', false)
    .option('--no-stdout', 'disable writing to stdout', true)
    .option('--no-color', 'disable color in output', false)
    .option('--no-rc', 'disable configuration from .remarkrc', false)
    .option('--no-ignore', 'disable ignore from .remarkignore', false);

/*
 * Listen.
 */

program.on('--help', help);

/**
 * CLI.
 *
 * @example
 *   var program = new CLI('--use lint .');
 *
 * @constructor
 * @class {CLI}
 * @param {Array.<string>|Object} argv - CLI arguments.
 */
function CLI(argv) {
    var self = this;
    var config = argv;

    /*
     * Allow passing in a CLI object.
     */

    if (argv instanceof CLI) {
        return argv;
    }

    self.cache = new Cache();
    self.spinner = new Spinner();
    self.injectedPlugins = [];

    if ('length' in config) {
        program.parse(argv);

        self.globs = program.args;
        self.extensions = [].concat(program.ext, EXTENSIONS);
        self.output = program.output;
        self.settings = program.setting;
        self.plugins = program.use;
        self.color = program.color;
        self.out = program.stdout;
        self.ast = program.ast;
        self.detectRC = program.rc;
        self.detectIgnore = program.ignore;
        self.quiet = program.quiet;
        self.silent = program.silent;
        self.frail = program.frail;
        self.filePath = program.filePath;
        self.configPath = program.configPath;
        self.ignorePath = program.ignorePath;
        self.watch = program.watch;
    } else {
        self.globs = [].concat(config.files || []);
        self.extensions = [].concat(config.extensions || [], EXTENSIONS);
        self.output = config.output;
        self.settings = config.settings;
        self.plugins = config.plugins;
        self.color = config.color;
        self.out = config.stdout;
        self.ast = config.ast;
        self.detectRC = config.detectRC;
        self.detectIgnore = config.detectIgnore;
        self.quiet = config.quiet;
        self.silent = config.silent;
        self.frail = config.frail;
        self.filePath = config.filePath;
        self.configPath = config.configPath;
        self.ignorePath = config.ignorePath;
        self.watch = config.watch;
    }

    self.cwd = config.cwd || process.cwd();

    Emitter.call(self);
}

/**
 * Log messages.
 *
 * @this {CLI}
 */
function log() {
    var self = this;
    var stdout = self.stdout;

    if (!self.silent && !self.quiet) {
        stdout.write.apply(stdout, arguments);
    }
}

/**
 * Output usage.
 *
 * @this {CLI}
 */
function usage() {
    return program.outputHelp();
}

/**
 * Cache plug-ins.
 *
 * @param {Function} plugin - Plugin to use.
 * @param {Object?} [options] - Optional plug-in
 *   configuration.
 * @this {CLI}
 */
function use(plugin, options) {
    this.injectedPlugins.push([plugin, options]);

    return this;
}

/*
 * Prototype.
 */

util.inherits(CLI, Emitter);

CLI.prototype.stdout = process.stdout;
CLI.prototype.stderr = process.stderr;
CLI.prototype.log = log;
CLI.prototype.usage = usage;
CLI.prototype.use = use;

/*
 * Expose.
 */

module.exports = CLI;
