/**
 * @fileoverview Main CLI object.
 * @author Nicholas C. Zakas
 * @copyright 2014 Nicholas C. Zakas. All rights reserved.
 */

"use strict";

/*
 * The CLI object should *not* call process.exit() directly. It should only return
 * exit codes. This allows other programs to use the CLI object and still control
 * when the program exits.
 */

//------------------------------------------------------------------------------
// Requirements
//------------------------------------------------------------------------------

var fs = require("fs"),
    path = require("path"),

    assign = require("object-assign"),
    debug = require("debug"),

    rules = require("./rules"),
    eslint = require("./eslint"),
    traverse = require("./util/traverse"),
    IgnoredPaths = require("./ignored-paths"),
    Config = require("./config"),
    util = require("./util");

//------------------------------------------------------------------------------
// Typedefs
//------------------------------------------------------------------------------

/**
 * The options to configure a CLI engine with.
 * @typedef {Object} CLIEngineOptions
 * @property {string} configFile The configuration file to use.
 * @property {boolean} reset True disables all default rules and environments.
 * @property {boolean|object} baseConfig Base config object. False disables all default rules and environments.
 * @property {boolean} ignore False disables use of .eslintignore.
 * @property {string[]} rulePaths An array of directories to load custom rules from.
 * @property {boolean} useEslintrc False disables looking for .eslintrc
 * @property {string[]} envs An array of environments to load.
 * @property {string[]} globals An array of global variables to declare.
 * @property {string[]} extensions An array of file extensions to check.
 * @property {Object<string,*>} rules An object of rules to use.
 * @property {string} ignorePath The ignore file to use instead of .eslintignore.
 */

/**
 * A linting warning or error.
 * @typedef {Object} LintMessage
 * @property {string} message The message to display to the user.
 */

/**
 * A linting result.
 * @typedef {Object} LintResult
 * @property {string} filePath The path to the file that was linted.
 * @property {LintMessage[]} messages All of the messages for the result.
 */

//------------------------------------------------------------------------------
// Private
//------------------------------------------------------------------------------


var defaultOptions = {
        configFile: null,
        reset: false,
        baseConfig: require(path.resolve(__dirname, "..", "conf", "eslint.json")),
        rulePaths: [],
        useEslintrc: true,
        envs: [],
        globals: [],
        rules: {},
        extensions: [".js"],
        ignore: true,
        ignorePath: null
    },
    loadedPlugins = Object.create(null);

//------------------------------------------------------------------------------
// Helpers
//------------------------------------------------------------------------------

debug = debug("eslint:cli-engine");

/**
 * Load the given plugins if they are not loaded already.
 * @param {string[]} pluginNames An array of plugin names which should be loaded.
 * @returns {void}
 */
function loadPlugins(pluginNames) {
    if (pluginNames) {
        pluginNames.forEach(function (pluginName) {
            var pluginNamespace = util.getNamespace(pluginName),
                pluginNameWithoutNamespace = util.removeNameSpace(pluginName),
                pluginNameWithoutPrefix = util.removePluginPrefix(pluginNameWithoutNamespace),
                plugin;

            if (!loadedPlugins[pluginNameWithoutPrefix]) {
                debug("Load plugin " + pluginNameWithoutPrefix);

                plugin = require(pluginNamespace + util.PLUGIN_NAME_PREFIX + pluginNameWithoutPrefix);
                // if this plugin has rules, import them
                if (plugin.rules) {
                    rules.import(plugin.rules, pluginNameWithoutPrefix);
                }

                loadedPlugins[pluginNameWithoutPrefix] = plugin;
            }
        });
    }
}

/**
 * It will calculate the error and warning count for collection of messages per file
 * @param {Object[]} messages - Collection of messages
 * @returns {Object} Contains the stats
 * @private
 */
function calculateStatsPerFile(messages) {
    return messages.reduce(function(stat, message) {
        if (message.fatal || message.severity === 2) {
            stat.errorCount++;
        } else {
            stat.warningCount++;
        }
        return stat;
    }, {
        errorCount: 0,
        warningCount: 0
    });
}

/**
 * It will calculate the error and warning count for collection of results from all files
 * @param {Object[]} results - Collection of messages from all the files
 * @returns {Object} Contains the stats
 * @private
 */
function calculateStatsPerRun(results) {
    return results.reduce(function(stat, result) {
        stat.errorCount += result.errorCount;
        stat.warningCount += result.warningCount;
        return stat;
    }, {
        errorCount: 0,
        warningCount: 0
    });
}

/**
 * Processes an individual file using ESLint. Files used here are known to
 * exist, so no need to check that here.
 * @param {string} filename The filename of the file being checked.
 * @param {Object} configHelper The configuration options for ESLint.
 * @returns {Result} The results for linting on this file.
 * @private
 */
function processFile(filename, configHelper) {

    // clear all existing settings for a new file
    eslint.reset();

    var filePath = path.resolve(filename),
        config,
        text,
        messages,
        stats,
        fileExtension = path.extname(filename),
        processor;

    debug("Linting " + filePath);
    config = configHelper.getConfig(filePath);
    loadPlugins(config.plugins);
    text = fs.readFileSync(path.resolve(filename), "utf8");

    for (var plugin in loadedPlugins) {
        if (loadedPlugins[plugin].processors && Object.keys(loadedPlugins[plugin].processors).indexOf(fileExtension) >= 0) {
            processor = loadedPlugins[plugin].processors[fileExtension];
            break;
        }
    }

    if (processor) {
        var parsedBlocks = processor.preprocess(text, filename);
        var unprocessedMessages = [];
        parsedBlocks.forEach(function(block) {
            unprocessedMessages.push(eslint.verify(block, config, filename));
        });
        messages = processor.postprocess(unprocessedMessages, filename);
    } else {
        messages = eslint.verify(text, config, filename);
    }

    stats = calculateStatsPerFile(messages);

    return {
        filePath: filename,
        messages: messages,
        errorCount: stats.errorCount,
        warningCount: stats.warningCount
    };
}

/**
 * Processes an source code using ESLint.
 * @param {string} text The source code to check.
 * @param {Object} configHelper The configuration options for ESLint.
 * @param {string} filename An optional string representing the texts filename.
 * @returns {Result} The results for linting on this text.
 * @private
 */
function processText(text, configHelper, filename) {

    // clear all existing settings for a new file
    eslint.reset();

    var config,
        messages,
        stats;

    filename = filename || "<text>";
    debug("Linting " + filename);
    config = configHelper.getConfig();
    loadPlugins(config.plugins);
    messages = eslint.verify(text, config, filename);

    stats = calculateStatsPerFile(messages);

    return {
        filePath: filename,
        messages: messages,
        errorCount: stats.errorCount,
        warningCount: stats.warningCount
    };
}

//------------------------------------------------------------------------------
// Public Interface
//------------------------------------------------------------------------------

/**
 * Creates a new instance of the core CLI engine.
 * @param {CLIEngineOptions} options The options for this instance.
 * @constructor
 */
function CLIEngine(options) {

    /**
     * Stored options for this instance
     * @type {Object}
     */
    this.options = assign(Object.create(defaultOptions), options || {});

    // load in additional rules
    if (this.options.rulePaths) {
        this.options.rulePaths.forEach(function(rulesdir) {
            debug("Loading rules from " + rulesdir);
            rules.load(rulesdir);
        });
    }

    loadPlugins(this.options.plugins);
}

CLIEngine.prototype = {

    constructor: CLIEngine,

    /**
     * Executes the current configuration on an array of file and directory names.
     * @param {string[]} files An array of file and directory names.
     * @returns {Object} The results for all files that were linted.
     */
    executeOnFiles: function(files) {

        var results = [],
            processed = [],
            options = this.options,
            configHelper = new Config(options),
            ignoredPaths = IgnoredPaths.load(options),
            exclude = ignoredPaths.contains.bind(ignoredPaths),
            stats;

        traverse({
            files: files,
            extensions: options.extensions,
            exclude: options.ignore ? exclude : false
        }, function(filename) {

            debug("Processing " + filename);

            processed.push(filename);
            results.push(processFile(filename, configHelper));
        });

        // only warn for files explicitly passed on the command line
        if (options.ignore) {
            files.forEach(function(file) {
                if (fs.statSync(path.resolve(file)).isFile() && processed.indexOf(file) === -1) {
                    results.push({
                        filePath: file,
                        messages: [
                            {
                                fatal: false,
                                severity: 1,
                                message: "File ignored because of your .eslintignore file. Use --no-ignore to override."
                            }
                        ],
                        errorCount: 0,
                        warningCount: 1
                    });
                }
            });
        }

        stats = calculateStatsPerRun(results);

        return {
            results: results,
            errorCount: stats.errorCount,
            warningCount: stats.warningCount
        };
    },

    /**
     * Executes the current configuration on text.
     * @param {string} text A string of JavaScript code to lint.
     * @param {string} filename An optional string representing the texts filename.
     * @returns {Object} The results for the linting.
     */
    executeOnText: function(text, filename) {

        var configHelper = new Config(this.options),
            results = [],
            stats;

        results.push(processText(text, configHelper, filename));
        stats = calculateStatsPerRun(results);

        return {
            results: results,
            errorCount: stats.errorCount,
            warningCount: stats.warningCount
        };
    },

    /**
     * Returns a configuration object for the given file based on the CLI options.
     * This is the same logic used by the ESLint CLI executable to determine
     * configuration for each file it processes.
     * @param {string} filePath The path of the file to retrieve a config object for.
     * @returns {Object} A configuration object for the file.
     */
    getConfigForFile: function(filePath) {
        var configHelper = new Config(this.options);
        return configHelper.getConfig(filePath);
    },

    /**
     * Checks if a given path is ignored by ESLint.
     * @param {string} filePath The path of the file to check.
     * @returns {boolean} Whether or not the given path is ignored.
     */
    isPathIgnored: function (filePath) {
        var ignoredPaths;

        if (this.options.ignore) {
            ignoredPaths = IgnoredPaths.load(this.options);
            return ignoredPaths.contains(filePath);
        }

        return false;
    },

    /**
     * Returns the formatter representing the given format or null if no formatter
     * with the given name can be found.
     * @param {string} [format] The name of the format to load or the path to a
     *      custom formatter.
     * @returns {Function} The formatter function or null if not found.
     */
    getFormatter: function(format) {

        var formatterPath;

        // default is stylish
        format = format || "stylish";

        // only strings are valid formatters
        if (typeof format === "string") {

            // replace \ with / for Windows compatibility
            format = format.replace(/\\/g, "/");

            // if there's a slash, then it's a file
            if (format.indexOf("/") > -1) {
                formatterPath = path.resolve(process.cwd(), format);
            } else {
                formatterPath = "./formatters/" + format;
            }

            try {
                return require(formatterPath);
            } catch (ex) {
                return null;
            }

        } else {
            return null;
        }


    }

};

module.exports = CLIEngine;
