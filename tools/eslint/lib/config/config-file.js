/**
 * @fileoverview Helper to locate and load configuration files.
 * @author Nicholas C. Zakas
 * @copyright 2015 Nicholas C. Zakas. All rights reserved.
 * See LICENSE file in root directory for full license.
 */
/* eslint no-use-before-define: 0 */
"use strict";

//------------------------------------------------------------------------------
// Requirements
//------------------------------------------------------------------------------

var debug = require("debug"),
    fs = require("fs"),
    path = require("path"),
    ConfigOps = require("./config-ops"),
    validator = require("./config-validator"),
    Plugins = require("./plugins"),
    resolveModule = require("resolve"),
    pathIsInside = require("path-is-inside"),
    stripComments = require("strip-json-comments"),
    stringify = require("json-stable-stringify"),
    isAbsolutePath = require("path-is-absolute");


//------------------------------------------------------------------------------
// Helpers
//------------------------------------------------------------------------------

/**
 * Determines sort order for object keys for json-stable-stringify
 *
 * see: https://github.com/substack/json-stable-stringify#cmp
 *
 * @param   {Object} a The first comparison object ({key: akey, value: avalue})
 * @param   {Object} b The second comparison object ({key: bkey, value: bvalue})
 * @returns {number}   1 or -1, used in stringify cmp method
 */
function sortByKey(a, b) {
    return a.key > b.key ? 1 : -1;
}

//------------------------------------------------------------------------------
// Private
//------------------------------------------------------------------------------

var CONFIG_FILES = [
    ".eslintrc.js",
    ".eslintrc.yaml",
    ".eslintrc.yml",
    ".eslintrc.json",
    ".eslintrc",
    "package.json"
];

debug = debug("eslint:config-file");

/**
 * Convenience wrapper for synchronously reading file contents.
 * @param {string} filePath The filename to read.
 * @returns {string} The file contents.
 * @private
 */
function readFile(filePath) {
    return fs.readFileSync(filePath, "utf8");
}

/**
 * Determines if a given string represents a filepath or not using the same
 * conventions as require(), meaning that the first character must be nonalphanumeric
 * and not the @ sign which is used for scoped packages to be considered a file path.
 * @param {string} filePath The string to check.
 * @returns {boolean} True if it's a filepath, false if not.
 * @private
 */
function isFilePath(filePath) {
    return isAbsolutePath(filePath) || !/\w|@/.test(filePath.charAt(0));
}

/**
 * Loads a YAML configuration from a file.
 * @param {string} filePath The filename to load.
 * @returns {Object} The configuration object from the file.
 * @throws {Error} If the file cannot be read.
 * @private
 */
function loadYAMLConfigFile(filePath) {
    debug("Loading YAML config file: " + filePath);

    // lazy load YAML to improve performance when not used
    var yaml = require("js-yaml");

    try {
        // empty YAML file can be null, so always use
        return yaml.safeLoad(readFile(filePath)) || {};
    } catch (e) {
        debug("Error reading YAML file: " + filePath);
        e.message = "Cannot read config file: " + filePath + "\nError: " + e.message;
        throw e;
    }
}

/**
 * Loads a JSON configuration from a file.
 * @param {string} filePath The filename to load.
 * @returns {Object} The configuration object from the file.
 * @throws {Error} If the file cannot be read.
 * @private
 */
function loadJSONConfigFile(filePath) {
    debug("Loading JSON config file: " + filePath);

    try {
        return JSON.parse(stripComments(readFile(filePath)));
    } catch (e) {
        debug("Error reading JSON file: " + filePath);
        e.message = "Cannot read config file: " + filePath + "\nError: " + e.message;
        throw e;
    }
}

/**
 * Loads a legacy (.eslintrc) configuration from a file.
 * @param {string} filePath The filename to load.
 * @returns {Object} The configuration object from the file.
 * @throws {Error} If the file cannot be read.
 * @private
 */
function loadLegacyConfigFile(filePath) {
    debug("Loading config file: " + filePath);

    // lazy load YAML to improve performance when not used
    var yaml = require("js-yaml");

    try {
        return yaml.safeLoad(stripComments(readFile(filePath))) || /* istanbul ignore next */ {};
    } catch (e) {
        debug("Error reading YAML file: " + filePath);
        e.message = "Cannot read config file: " + filePath + "\nError: " + e.message;
        throw e;
    }
}

/**
 * Loads a JavaScript configuration from a file.
 * @param {string} filePath The filename to load.
 * @returns {Object} The configuration object from the file.
 * @throws {Error} If the file cannot be read.
 * @private
 */
function loadJSConfigFile(filePath) {
    debug("Loading JS config file: " + filePath);
    try {
        return require(filePath);
    } catch (e) {
        debug("Error reading JavaScript file: " + filePath);
        e.message = "Cannot read config file: " + filePath + "\nError: " + e.message;
        throw e;
    }
}

/**
 * Loads a configuration from a package.json file.
 * @param {string} filePath The filename to load.
 * @returns {Object} The configuration object from the file.
 * @throws {Error} If the file cannot be read.
 * @private
 */
function loadPackageJSONConfigFile(filePath) {
    debug("Loading package.json config file: " + filePath);
    try {
        return loadJSONConfigFile(filePath).eslintConfig || null;
    } catch (e) {
        debug("Error reading package.json file: " + filePath);
        e.message = "Cannot read config file: " + filePath + "\nError: " + e.message;
        throw e;
    }
}

/**
 * Loads a configuration file regardless of the source. Inspects the file path
 * to determine the correctly way to load the config file.
 * @param {Object} file The path to the configuration.
 * @returns {Object} The configuration information.
 * @private
 */
function loadConfigFile(file) {
    var config;

    var filePath = file.filePath;

    switch (path.extname(filePath)) {
        case ".js":
            config = loadJSConfigFile(filePath);
            if (file.configName) {
                config = config.configs[file.configName];
            }
            break;

        case ".json":
            if (path.basename(filePath) === "package.json") {
                config = loadPackageJSONConfigFile(filePath);
                if (config === null) {
                    return null;
                }
            } else {
                config = loadJSONConfigFile(filePath);
            }
            break;

        case ".yaml":
        case ".yml":
            config = loadYAMLConfigFile(filePath);
            break;

        default:
            config = loadLegacyConfigFile(filePath);
    }

    return ConfigOps.merge(ConfigOps.createEmptyConfig(), config);
}

/**
 * Writes a configuration file in JSON format.
 * @param {Object} config The configuration object to write.
 * @param {string} filePath The filename to write to.
 * @returns {void}
 * @private
 */
function writeJSONConfigFile(config, filePath) {
    debug("Writing JSON config file: " + filePath);

    var content = stringify(config, {cmp: sortByKey, space: 4});
    fs.writeFileSync(filePath, content, "utf8");
}

/**
 * Writes a configuration file in YAML format.
 * @param {Object} config The configuration object to write.
 * @param {string} filePath The filename to write to.
 * @returns {void}
 * @private
 */
function writeYAMLConfigFile(config, filePath) {
    debug("Writing YAML config file: " + filePath);

    // lazy load YAML to improve performance when not used
    var yaml = require("js-yaml");

    var content = yaml.safeDump(config, {sortKeys: true});
    fs.writeFileSync(filePath, content, "utf8");
}

/**
 * Writes a configuration file in JavaScript format.
 * @param {Object} config The configuration object to write.
 * @param {string} filePath The filename to write to.
 * @returns {void}
 * @private
 */
function writeJSConfigFile(config, filePath) {
    debug("Writing JS config file: " + filePath);

    var content = "module.exports = " + stringify(config, {cmp: sortByKey, space: 4}) + ";";
    fs.writeFileSync(filePath, content, "utf8");
}

/**
 * Writes a configuration file.
 * @param {Object} config The configuration object to write.
 * @param {string} filePath The filename to write to.
 * @returns {void}
 * @throws {Error} When an unknown file type is specified.
 * @private
 */
function write(config, filePath) {
    switch (path.extname(filePath)) {
        case ".js":
            writeJSConfigFile(config, filePath);
            break;

        case ".json":
            writeJSONConfigFile(config, filePath);
            break;

        case ".yaml":
        case ".yml":
            writeYAMLConfigFile(config, filePath);
            break;

        default:
            throw new Error("Can't write to unknown file type.");
    }
}

/**
 * Determines the lookup path for node packages referenced in a config file.
 * If the config
 * @param {string} configFilePath The config file referencing the file.
 * @returns {string} The lookup path for the file path.
 * @private
 */
function getLookupPath(configFilePath) {

    // calculates the path of the project including ESLint as dependency
    var projectPath = path.resolve(__dirname, "../../../");
    if (configFilePath && pathIsInside(configFilePath, projectPath)) {
        // be careful of https://github.com/substack/node-resolve/issues/78
        return path.resolve(configFilePath);
    }

    // default to ESLint project path since it's unlikely that plugins will be
    // in this directory
    return projectPath;
}

/**
 * Applies values from the "extends" field in a configuration file.
 * @param {Object} config The configuration information.
 * @param {string} filePath The file path from which the configuration information
 *      was loaded.
 * @returns {Object} A new configuration object with all of the "extends" fields
 *      loaded and merged.
 * @private
 */
function applyExtends(config, filePath) {
    var configExtends = config.extends;

    // normalize into an array for easier handling
    if (!Array.isArray(config.extends)) {
        configExtends = [config.extends];
    }

    // Make the last element in an array take the highest precedence
    config = configExtends.reduceRight(function(previousValue, parentPath) {

        if (parentPath === "eslint:recommended") {
            // Add an explicit substitution for eslint:recommended to conf/eslint.json
            // this lets us use the eslint.json file as the recommended rules
            parentPath = path.resolve(__dirname, "../../conf/eslint.json");
        } else if (isFilePath(parentPath)) {
            // If the `extends` path is relative, use the directory of the current configuration
            // file as the reference point. Otherwise, use as-is.
            parentPath = (!isAbsolutePath(parentPath) ?
                path.join(path.dirname(filePath), parentPath) :
                parentPath
            );
        }

        try {
            debug("Loading " + parentPath);
            return ConfigOps.merge(load(parentPath), previousValue);
        } catch (e) {
            // If the file referenced by `extends` failed to load, add the path to the
            // configuration file that referenced it to the error message so the user is
            // able to see where it was referenced from, then re-throw
            e.message += "\nReferenced from: " + filePath;
            throw e;
        }

    }, config);

    return config;
}

/**
 * Brings package name to correct format based on prefix
 * @param {string} name The name of the package.
 * @param {string} prefix Can be either "eslint-plugin" or "eslint-config
 * @returns {string} Normalized name of the package
 * @private
 */
function normalizePackageName(name, prefix) {
    if (name.charAt(0) === "@") {
        // it's a scoped package
        // package name is "eslint-config", or just a username
        var scopedPackageShortcutRegex = new RegExp("^(@[^\/]+)(?:\/(?:" + prefix + ")?)?$"),
            scopedPackageNameRegex = new RegExp("^" + prefix + "(-|$)");
        if (scopedPackageShortcutRegex.test(name)) {
            name = name.replace(scopedPackageShortcutRegex, "$1/" + prefix);
        } else if (!scopedPackageNameRegex.test(name.split("/")[1])) {
            // for scoped packages, insert the eslint-config after the first / unless
            // the path is already @scope/eslint or @scope/eslint-config-xxx
            name = name.replace(/^@([^\/]+)\/(.*)$/, "@$1/" + prefix + "-$2");
        }
    } else if (name.indexOf(prefix + "-") !== 0) {
        name = prefix + "-" + name;
    }

    return name;
}

/**
 * Resolves a configuration file path into the fully-formed path, whether filename
 * or package name.
 * @param {string} filePath The filepath to resolve.
 * @param {string} [relativeTo] The path to resolve relative to.
 * @returns {Object} A path that can be used directly to load the configuration.
 * @private
 */
function resolve(filePath, relativeTo) {

    if (isFilePath(filePath)) {
        return { filePath: path.resolve(relativeTo || "", filePath) };
    } else {
        if (filePath.indexOf("plugin:") === 0) {
            var packagePath = filePath.substr(7, filePath.lastIndexOf("/") - 7);
            var configName = filePath.substr(filePath.lastIndexOf("/") + 1, filePath.length - filePath.lastIndexOf("/") - 1);
            filePath = resolveModule.sync(normalizePackageName(packagePath, "eslint-plugin"), {
                basedir: getLookupPath(relativeTo)
            });
            return { filePath: filePath, configName: configName };
        } else {
            filePath = resolveModule.sync(normalizePackageName(filePath, "eslint-config"), {
                basedir: getLookupPath(relativeTo)
            });
            return { filePath: filePath };
        }
    }

}

/**
 * Loads a configuration file from the given file path.
 * @param {string} filePath The filename or package name to load the configuration
 *      information from.
 * @param {boolean} [applyEnvironments=false] Set to true to merge in environment settings.
 * @returns {Object} The configuration information.
 * @private
 */
function load(filePath, applyEnvironments) {

    var resolvedPath = resolve(filePath),
        config = loadConfigFile(resolvedPath);

    if (config) {

        // ensure plugins are properly loaded first
        if (config.plugins) {
            Plugins.loadAll(config.plugins);
        }

        // include full path of parser if present
        if (config.parser) {
            config.parser = resolveModule.sync(config.parser, {
                basedir: getLookupPath(path.dirname(path.resolve(filePath)))
            });
        }

        // validate the configuration before continuing
        validator.validate(config, filePath);

        // If an `extends` property is defined, it represents a configuration file to use as
        // a "parent". Load the referenced file and merge the configuration recursively.
        if (config.extends) {
            config = applyExtends(config, filePath);
        }

        if (config.env && applyEnvironments) {
            // Merge in environment-specific globals and parserOptions.
            config = ConfigOps.applyEnvironments(config);
        }

    }

    return config;
}

//------------------------------------------------------------------------------
// Public Interface
//------------------------------------------------------------------------------

module.exports = {

    getLookupPath: getLookupPath,
    load: load,
    resolve: resolve,
    write: write,
    applyExtends: applyExtends,
    CONFIG_FILES: CONFIG_FILES,

    /**
     * Retrieves the configuration filename for a given directory. It loops over all
     * of the valid configuration filenames in order to find the first one that exists.
     * @param {string} directory The directory to check for a config file.
     * @returns {?string} The filename of the configuration file for the directory
     *      or null if there is no configuration file in the directory.
     */
    getFilenameForDirectory: function(directory) {

        var filename;

        for (var i = 0, len = CONFIG_FILES.length; i < len; i++) {
            filename = path.join(directory, CONFIG_FILES[i]);
            if (fs.existsSync(filename)) {
                return filename;
            }
        }

        return null;
    }
};
