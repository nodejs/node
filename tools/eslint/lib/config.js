/**
 * @fileoverview Responsible for loading config files
 * @author Seth McLaughlin
 */

"use strict";

//------------------------------------------------------------------------------
// Requirements
//------------------------------------------------------------------------------

var path = require("path"),
    ConfigOps = require("./config/config-ops"),
    ConfigFile = require("./config/config-file"),
    Plugins = require("./config/plugins"),
    FileFinder = require("./file-finder"),
    debug = require("debug"),
    userHome = require("user-home"),
    isResolvable = require("is-resolvable"),
    pathIsInside = require("path-is-inside");

//------------------------------------------------------------------------------
// Constants
//------------------------------------------------------------------------------

var PERSONAL_CONFIG_DIR = userHome || null;

//------------------------------------------------------------------------------
// Helpers
//------------------------------------------------------------------------------

debug = debug("eslint:config");

/**
 * Check if item is an javascript object
 * @param {*} item object to check for
 * @returns {boolean} True if its an object
 * @private
 */
function isObject(item) {
    return typeof item === "object" && !Array.isArray(item) && item !== null;
}

/**
 * Load and parse a JSON config object from a file.
 * @param {string|Object} configToLoad the path to the JSON config file or the config object itself.
 * @returns {Object} the parsed config object (empty object if there was a parse error)
 * @private
 */
function loadConfig(configToLoad) {
    var config = {},
        filePath = "";

    if (configToLoad) {

        if (isObject(configToLoad)) {
            config = configToLoad;

            if (config.extends) {
                config = ConfigFile.applyExtends(config, filePath);
            }
        } else {
            filePath = configToLoad;
            config = ConfigFile.load(filePath);
        }

    }

    return config;
}

/**
 * Get personal config object from ~/.eslintrc.
 * @returns {Object} the personal config object (empty object if there is no personal config)
 * @private
 */
function getPersonalConfig() {
    var config,
        filename;

    if (PERSONAL_CONFIG_DIR) {
        filename = ConfigFile.getFilenameForDirectory(PERSONAL_CONFIG_DIR);

        if (filename) {
            debug("Using personal config");
            config = loadConfig(filename);
        }
    }

    return config || {};
}

/**
 * Get a local config object.
 * @param {Object} thisConfig A Config object.
 * @param {string} directory The directory to start looking in for a local config file.
 * @returns {Object} The local config object, or an empty object if there is no local config.
 */
function getLocalConfig(thisConfig, directory) {
    var found,
        i,
        localConfig,
        localConfigFile,
        config = {},
        localConfigFiles = thisConfig.findLocalConfigFiles(directory),
        numFiles = localConfigFiles.length,
        rootPath,
        projectConfigPath = ConfigFile.getFilenameForDirectory(thisConfig.options.cwd);

    for (i = 0; i < numFiles; i++) {

        localConfigFile = localConfigFiles[i];

        // Don't consider the personal config file in the home directory,
        // except if the home directory is the same as the current working directory
        if (path.dirname(localConfigFile) === PERSONAL_CONFIG_DIR && localConfigFile !== projectConfigPath) {
            continue;
        }

        // If root flag is set, don't consider file if it is above root
        if (rootPath && !pathIsInside(path.dirname(localConfigFile), rootPath)) {
            continue;
        }

        debug("Loading " + localConfigFile);
        localConfig = loadConfig(localConfigFile);

        // Don't consider a local config file found if the config is null
        if (!localConfig) {
            continue;
        }

        // Check for root flag
        if (localConfig.root === true) {
            rootPath = path.dirname(localConfigFile);
        }

        found = true;
        debug("Using " + localConfigFile);
        config = ConfigOps.merge(localConfig, config);
    }

    // Use the personal config file if there are no other local config files found.
    return found || thisConfig.useSpecificConfig ? config : ConfigOps.merge(config, getPersonalConfig());
}

//------------------------------------------------------------------------------
// API
//------------------------------------------------------------------------------

/**
 * Config
 * @constructor
 * @class Config
 * @param {Object} options Options to be passed in
 */
function Config(options) {
    var useConfig;

    options = options || {};

    this.ignore = options.ignore;
    this.ignorePath = options.ignorePath;
    this.cache = {};
    this.parser = options.parser;
    this.parserOptions = options.parserOptions || {};

    this.baseConfig = options.baseConfig ? loadConfig(options.baseConfig) : { rules: {} };

    this.useEslintrc = (options.useEslintrc !== false);

    this.env = (options.envs || []).reduce(function(envs, name) {
        envs[name] = true;
        return envs;
    }, {});

    /*
     * Handle declared globals.
     * For global variable foo, handle "foo:false" and "foo:true" to set
     * whether global is writable.
     * If user declares "foo", convert to "foo:false".
     */
    this.globals = (options.globals || []).reduce(function(globals, def) {
        var parts = def.split(":");

        globals[parts[0]] = (parts.length > 1 && parts[1] === "true");

        return globals;
    }, {});

    useConfig = options.configFile;
    this.options = options;

    if (useConfig) {
        debug("Using command line config " + useConfig);
        if (isResolvable(useConfig) || isResolvable("eslint-config-" + useConfig) || useConfig.charAt(0) === "@") {
            this.useSpecificConfig = loadConfig(useConfig);
        } else {
            this.useSpecificConfig = loadConfig(path.resolve(this.options.cwd, useConfig));
        }
    }
}

/**
 * Build a config object merging the base config (conf/eslint.json), the
 * environments config (conf/environments.js) and eventually the user config.
 * @param {string} filePath a file in whose directory we start looking for a local config
 * @returns {Object} config object
 */
Config.prototype.getConfig = function(filePath) {
    var config,
        userConfig,
        directory = filePath ? path.dirname(filePath) : this.options.cwd;

    debug("Constructing config for " + (filePath ? filePath : "text"));

    config = this.cache[directory];

    if (config) {
        debug("Using config from cache");
        return config;
    }

    // Step 1: Determine user-specified config from .eslintrc.* and package.json files
    if (this.useEslintrc) {
        debug("Using .eslintrc and package.json files");
        userConfig = getLocalConfig(this, directory);
    } else {
        debug("Not using .eslintrc or package.json files");
        userConfig = {};
    }

    // Step 2: Create a copy of the baseConfig
    config = ConfigOps.merge({parser: this.parser, parserOptions: this.parserOptions}, this.baseConfig);

    // Step 3: Merge in the user-specified configuration from .eslintrc and package.json
    config = ConfigOps.merge(config, userConfig);

    // Step 4: Merge in command line config file
    if (this.useSpecificConfig) {
        debug("Merging command line config file");

        config = ConfigOps.merge(config, this.useSpecificConfig);
    }

    // Step 5: Merge in command line environments
    debug("Merging command line environment settings");
    config = ConfigOps.merge(config, { env: this.env });

    // Step 6: Merge in command line rules
    if (this.options.rules) {
        debug("Merging command line rules");
        config = ConfigOps.merge(config, { rules: this.options.rules });
    }

    // Step 7: Merge in command line globals
    config = ConfigOps.merge(config, { globals: this.globals });

    // Step 8: Merge in command line plugins
    if (this.options.plugins) {
        debug("Merging command line plugins");
        Plugins.loadAll(this.options.plugins);
        config = ConfigOps.merge(config, { plugins: this.options.plugins });
    }

    // Step 9: Apply environments to the config if present
    if (config.env) {
        config = ConfigOps.applyEnvironments(config);
    }

    this.cache[directory] = config;

    return config;
};

/**
 * Find local config files from directory and parent directories.
 * @param {string} directory The directory to start searching from.
 * @returns {string[]} The paths of local config files found.
 */
Config.prototype.findLocalConfigFiles = function(directory) {

    if (!this.localConfigFinder) {
        this.localConfigFinder = new FileFinder(ConfigFile.CONFIG_FILES, this.options.cwd);
    }

    return this.localConfigFinder.findAllInDirectoryAndParents(directory);
};

module.exports = Config;
