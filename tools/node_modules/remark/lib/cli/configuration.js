/**
 * @author Titus Wormer
 * @copyright 2015-2016 Titus Wormer
 * @license MIT
 * @module remark:cli:configuration
 * @version 3.2.2
 * @fileoverview Find remark rc files.
 */

'use strict';

/* eslint-env node */

/*
 * Dependencies.
 */

var fs = require('fs');
var path = require('path');
var debug = require('debug')('remark:cli:configuration');
var home = require('user-home');
var findUp = require('vfile-find-up');
var extend = require('extend.js');
var defaults = require('../defaults');

/*
 * Constants.
 */

var RC_NAME = '.remarkrc';
var PLUGIN_KEY = 'plugins';
var PACKAGE_NAME = 'package';
var PACKAGE_EXTENSION = 'json';
var PACKAGE_FILENAME = [PACKAGE_NAME, PACKAGE_EXTENSION].join('.');
var PACKAGE_FIELD = 'remarkConfig';
var PERSONAL_CONFIGURATION = home ? path.join(home, RC_NAME) : null;

/*
 * Methods.
 */

var read = fs.readFileSync;
var exists = fs.existsSync;
var has = Object.prototype.hasOwnProperty;
var concat = Array.prototype.concat;

/*
 * Set-up.
 */

var base = {
    'settings': {}
};

extend(base.settings, defaults.parse);
extend(base.settings, defaults.stringify);

/**
 * Merge two configurations, `configuration` into
 * `target`.
 *
 * @example
 *   var target = {};
 *   merge(target, current);
 *
 * @param {Object} target - Configuration to merge into.
 * @param {Object} configuration - Configuration to merge
 *   from.
 * @param {boolean} [recursive] - Used internally no ensure
 *   the plug-in key is only escaped at root-level.
 * @return {Object} - `target`.
 */
function merge(target, configuration, recursive) {
    var key;
    var value;
    var index;
    var length;
    var result;

    for (key in configuration) {
        if (has.call(configuration, key)) {
            value = configuration[key];
            result = target[key];

            if (key === PLUGIN_KEY && !recursive) {
                if (!result) {
                    result = {};

                    target[key] = result;
                }

                if ('length' in value) {
                    index = -1;
                    length = value.length;

                    while (++index < length) {
                        if (!(value[index] in result)) {
                            result[value[index]] = null;
                        }
                    }
                } else {
                    target[key] = merge(result, value, true);
                }
            } else if (typeof value === 'object' && value !== null) {
                if ('length' in value) {
                    target[key] = concat.apply(value);
                } else {
                    target[key] = merge(result || {}, value, true);
                }
            } else if (value !== undefined) {
                target[key] = value;
            }
        }
    }

    return target;
}

/**
 * Parse a JSON configuration object from a file.
 *
 * @example
 *   var rawConfig = load('package.json');
 *
 * @throws {Error} - Throws when `filePath` is not found.
 * @param {string} filePath - File location.
 * @return {Object} - Parsed JSON.
 */
function load(filePath) {
    var configuration = {};

    if (filePath) {
        try {
            configuration = JSON.parse(read(filePath, 'utf8')) || {};
        } catch (exception) {
            exception.message = 'Cannot read configuration file: ' +
                filePath + '\n' + exception.message;

            throw exception;
        }
    }

    return configuration;
}

/**
 * Get personal configuration object from `~`.
 *
 * @example
 *   var config = getUserConfiguration();
 *
 * @return {Object} - Parsed JSON.
 */
function getUserConfiguration() {
    var configuration = {};

    if (PERSONAL_CONFIGURATION && exists(PERSONAL_CONFIGURATION)) {
        configuration = load(PERSONAL_CONFIGURATION);
    }

    return configuration;
}

/**
 * Get a local configuration object, by walking from
 * `directory` upwards and mergin all configurations.
 * If no configuration was found by walking upwards, the
 * current user's config (at `~`) is used.
 *
 * @example
 *   var configuration = new Configuration();
 *   var config = getLocalConfiguration(configuration, '~/bar');
 *
 * @param {Configuration} context - Configuration object to use.
 * @param {string} directory - Location to search.
 * @param {Function} callback - Invoked with `files`.
 */
function getLocalConfiguration(context, directory, callback) {
    findUp.all([RC_NAME, PACKAGE_FILENAME], directory, function (err, files) {
        var configuration = {};
        var index = files && files.length;
        var file;
        var local;
        var found;

        while (index--) {
            file = files[index];

            local = load(file.filePath());

            if (
                file.filename === PACKAGE_NAME &&
                file.extension === PACKAGE_EXTENSION
            ) {
                local = local[PACKAGE_FIELD] || {};
            }

            found = true;

            debug('Using ' + file.filePath());

            merge(configuration, local);
        }

        if (!found) {
            debug('Using personal configuration');

            merge(configuration, getUserConfiguration());
        }

        callback(err, configuration);
    });
}

/**
 * Configuration.
 *
 * @example
 *   var configuration = new Configuration();
 *
 * @constructor
 * @class Configuration
 * @param {Object} options - Options to be passed in.
 */
function Configuration(options) {
    var self = this;
    var settings = options || {};
    var file = settings.file;
    var cliConfiguration = {};

    self.cache = {};

    self.cwd = settings.cwd || process.cwd();

    self.settings = settings.settings || {};
    self.plugins = settings.plugins || {};
    self.output = settings.output;
    self.detectRC = settings.detectRC;

    if (file) {
        debug('Using command line configuration `' + file + '`');

        cliConfiguration = load(path.resolve(self.cwd, file));
    }

    self.cliConfiguration = cliConfiguration;
}

/**
 * Defaults.
 *
 * @type {Object} - Default settings.
 */
Configuration.prototype.base = base;

/**
 * Build a configuration object.
 *
 * @example
 *   new Configuration().getConfiguration('~/foo', console.log);
 *
 * @param {string} filePath - File location.
 * @param {Function} callback - Callback invoked with
 *   configuration.
 */
Configuration.prototype.getConfiguration = function (filePath, callback) {
    var self = this;
    var directory = filePath ? path.dirname(filePath) : self.cwd;
    var configuration = self.cache[directory];

    debug('Constructing configuration for `' + (filePath || self.cwd) + '`');

    /**
     * Handle (possible) local config result.
     *
     * @param {Error?} [err] - Loading error.
     * @param {Object?} [localConfiguration] - Configuration.
     */
    function handleLocalConfiguration(err, localConfiguration) {
        if (localConfiguration) {
            merge(configuration, localConfiguration);
        }

        merge(configuration, self.cliConfiguration);

        merge(configuration, {
            'settings': self.settings,
            'plugins': self.plugins,
            'output': self.output
        });

        self.cache[directory] = configuration;

        callback(err, configuration);
    }

    if (configuration) {
        debug('Using configuration from cache');
        callback(null, configuration);
    } else {
        configuration = {};

        merge(configuration, self.base);

        if (!self.detectRC) {
            debug('Ignoring .rc files');
            handleLocalConfiguration();
        } else {
            getLocalConfiguration(self, directory, handleLocalConfiguration);
        }
    }
};

/*
 * Expose.
 */

module.exports = Configuration;
