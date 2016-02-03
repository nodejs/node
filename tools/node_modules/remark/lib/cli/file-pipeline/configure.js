/**
 * @author Titus Wormer
 * @copyright 2015-2016 Titus Wormer
 * @license MIT
 * @module remark:cli:file-pipeline:configure
 * @version 3.2.2
 * @fileoverview Configure a file.
 */

'use strict';

/* eslint-env node */

/*
 * Dependencies.
 */

var fs = require('fs');
var path = require('path');
var debug = require('debug')('remark:cli:file-pipeline:configure');
var npmPrefix = require('npm-prefix')();
var remark = require('../../..');

/*
 * Methods.
 */

var exists = fs.existsSync;
var join = path.join;
var resolve = path.resolve;

/*
 * Constants.
 */

var SEPERATOR = path.sep;
var MODULES = 'node_modules';
var isWindows = process.platform === 'win32';
var isGlobal = process.argv[1].indexOf(npmPrefix) === 0;
var globals = resolve(npmPrefix, isWindows ? '' : 'lib', MODULES);

/*
 * Utilities.
 */

/**
 * Find root of a node module: parent directory of
 * `package.json`, or, the given directory if no
 * ancestral `package.json` is found.
 *
 * @example
 *   findRoot('remark/test'); // 'remark'
 *
 * @todo Externalise.
 * @param {string} base - Path to directory.
 * @return {string} - Path to an ancestral project
 *   directory.
 */
function findRoot(base) {
    var location = base;
    var parts = base.split(SEPERATOR);

    while (!exists(join(location, 'package.json')) && parts.length > 1) {
        parts.pop();

        location = parts.join(SEPERATOR);
    }

    return parts.length ? location : base;
}

/**
 * Require a plugin.  Checks, in this order:
 *
 * -  `$package/$pathlike`;
 * -  `$package/$pathlike.js`;
 * -  `$package/node_modules/$pathlike`;
 * -  `$package/node_modules/remark-$pathlike`;
 * -  `$cwd/node_modules/$pathlike`;
 * -  `$cwd/node_modules/remark-$pathlike`;
 * -  `$pathlike`.
 *
 * Where `$package` is an ancestral package directory.
 *
 * When using a globally installed executable, the
 * following are also included:
 *
 * -  `$modules/$pathlike`;
 * -  `$modules/remark-$pathlike`.
 *
 * Where `$modules` is the directory of globally installed
 * npm packages.
 *
 * @see https://docs.npmjs.com/files/folders#node-modules
 *
 * @example
 *   var plugin = findPlugin('toc');
 *
 * @todo Externalise.
 * @throws {Error} - Fails when `pathlike` cannot be
 *   resolved.
 * @param {string} pathlike - Reference to plugin.
 * @param {string?} [cwd] - Relative working directory,
 *   defaults to the current working directory.
 * @return {Object} - Result of `require`ing `plugin`.
 */
function findPlugin(pathlike, cwd) {
    var root = findRoot(cwd);
    var pluginlike = 'remark-' + pathlike;
    var index = -1;
    var plugin = pathlike;
    var length;
    var paths = [
        resolve(root, pathlike),
        resolve(root, pathlike + '.js'),
        resolve(root, MODULES, pluginlike),
        resolve(root, MODULES, pathlike),
        resolve(cwd, MODULES, pluginlike),
        resolve(cwd, MODULES, pathlike)
    ];

    if (isGlobal) {
        paths.push(
            resolve(globals, pathlike),
            resolve(globals, pluginlike)
        );
    }

    length = paths.length;

    while (++index < length) {
        if (exists(paths[index])) {
            plugin = paths[index];
            break;
        }
    }

    debug('Using plug-in `%s` at `%s`', pathlike, plugin);

    return require(plugin);
}

/**
 * Collect configuration for a file based on the context.
 *
 * @example
 *   var fileSet = new FileSet(cli);
 *   var file = new File({
 *     'directory': '~',
 *     'filename': 'example',
 *     'extension': 'md'
 *   });
 *
 *   configure({'file': file, 'fileSet': fileSet});
 *
 * @param {Object} context - Context object.
 * @param {function(Error?)} next - Callback invoked when
 *   done.
 */
function configure(context, next) {
    var file = context.file;
    var cli = context.fileSet.cli;
    var config = cli.configuration;
    var processor = remark();
    var plugins;

    if (file.hasFailed()) {
        next();
        return;
    }

    config.getConfiguration(file.filePath(), function (err, options) {
        var option;
        var plugin;
        var length;
        var index;
        var name;

        debug('Setting output `%s`', options.output);

        debug('Using settings `%j`', options.settings);

        plugins = Object.keys(options.plugins);
        length = plugins.length;
        index = -1;

        debug('Using plug-ins `%j`', plugins);

        while (++index < length) {
            name = plugins[index];
            option = options.plugins[name];

            if (option === false) {
                debug('Ignoring plug-in `%s`', name);
                continue;
            }

            /*
             * Allow for default arguments.
             */

            if (option === null) {
                option = undefined;
            }

            try {
                plugin = findPlugin(name, cli.cwd);

                debug('Applying options `%j` to `%s`', option, name);

                processor.use(plugin, option, context.fileSet);
            } catch (err) {
                next(err);
                return;
            }
        }

        plugins = cli.injectedPlugins;
        length = plugins.length;
        index = -1;

        debug('Using `%d` injected plugins', length);

        while (++index < length) {
            plugin = plugins[index][0];
            option = plugins[index][1];
            name = plugin.displayName || plugin.name || '';

            try {
                debug('Applying options `%j` to `%s`', option, name);

                processor.use(plugin, option, context.fileSet);
            } catch (err) {
                next(err);
                return;
            }
        }

        /*
         * Store some configuration on the context object.
         */

        context.output = options.output;
        context.settings = options.settings;
        context.processor = processor;

        next();
    });
}

/*
 * Expose.
 */

module.exports = configure;
