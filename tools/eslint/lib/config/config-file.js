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
    stripComments = require("strip-json-comments"),
    isAbsolutePath = require("path-is-absolute");

//------------------------------------------------------------------------------
// Private
//------------------------------------------------------------------------------

var CONFIG_FILES = [
    ".eslintrc.js",
    ".eslintrc.yaml",
    ".eslintrc.yml",
    ".eslintrc.json",
    ".eslintrc"
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
        return yaml.safeLoad(stripComments(readFile(filePath))) || {};
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
        return require(filePath).eslintConfig || null;
    } catch (e) {
        debug("Error reading package.json file: " + filePath);
        e.message = "Cannot read config file: " + filePath + "\nError: " + e.message;
        throw e;
    }
}

/**
 * Loads a JavaScript configuration from a package.
 * @param {string} filePath The package name to load.
 * @returns {Object} The configuration object from the package.
 * @throws {Error} If the package cannot be read.
 * @private
 */
function loadPackage(filePath) {
    debug("Loading config package: " + filePath);
    try {
        return require(filePath);
    } catch (e) {
        debug("Error reading package: " + filePath);
        e.message = "Cannot read config package: " + filePath + "\nError: " + e.message;
        throw e;
    }
}

/**
 * Loads a configuration file regardless of the source. Inspects the file path
 * to determine the correctly way to load the config file.
 * @param {string} filePath The path to the configuration.
 * @returns {Object} The configuration information.
 * @private
 */
function loadConfigFile(filePath) {
    var config;

    if (isFilePath(filePath)) {
        switch (path.extname(filePath)) {
            case ".js":
                config = loadJSConfigFile(filePath);
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
    } else {
        config = loadPackage(filePath);
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

    var content = JSON.stringify(config, null, 4);
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

    var content = yaml.safeDump(config);
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

    var content = "module.exports = " + JSON.stringify(config, null, 4) + ";";
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
 * Resolves a configuration file path into the fully-formed path, whether filename
 * or package name.
 * @param {string} filePath The filepath to resolve.
 * @returns {string} A path that can be used directly to load the configuration.
 * @private
 */
function resolve(filePath) {

    if (isFilePath(filePath)) {
        return path.resolve(filePath);
    } else {

        // it's a package

        if (filePath.charAt(0) === "@") {
            // it's a scoped package

            // package name is "eslint-config", or just a username
            var scopedPackageShortcutRegex = /^(@[^\/]+)(?:\/(?:eslint-config)?)?$/;
            if (scopedPackageShortcutRegex.test(filePath)) {
                filePath = filePath.replace(scopedPackageShortcutRegex, "$1/eslint-config");
            } else if (filePath.split("/")[1].indexOf("eslint-config-") !== 0) {
                // for scoped packages, insert the eslint-config after the first /
                filePath = filePath.replace(/^@([^\/]+)\/(.*)$/, "@$1/eslint-config-$2");
            }
        } else if (filePath.indexOf("eslint-config-") !== 0) {
            filePath = "eslint-config-" + filePath;
        }

        return filePath;
    }

}

/**
 * Loads a configuration file from the given file path.
 * @param {string} filePath The filename or package name to load the configuration
 *      information from.
 * @returns {Object} The configuration information.
 * @private
 */
function load(filePath) {

    var resolvedPath = resolve(filePath),
        config = loadConfigFile(resolvedPath);

    if (config) {

        // validate the configuration before continuing
        validator.validate(config, filePath);

        // If an `extends` property is defined, it represents a configuration file to use as
        // a "parent". Load the referenced file and merge the configuration recursively.
        if (config.extends) {
            config = applyExtends(config, filePath);
        }

        if (config.env) {
            // Merge in environment-specific globals and ecmaFeatures.
            config = ConfigOps.applyEnvironments(config);
        }

    }

    return config;
}

//------------------------------------------------------------------------------
// Public Interface
//------------------------------------------------------------------------------

module.exports = {

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
