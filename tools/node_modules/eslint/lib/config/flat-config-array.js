/**
 * @fileoverview Flat Config Array
 * @author Nicholas C. Zakas
 */

"use strict";

//-----------------------------------------------------------------------------
// Requirements
//-----------------------------------------------------------------------------

const { ConfigArray, ConfigArraySymbol } = require("@humanwhocodes/config-array");
const { flatConfigSchema } = require("./flat-config-schema");
const { RuleValidator } = require("./rule-validator");
const { defaultConfig } = require("./default-config");
const jsPlugin = require("@eslint/js");

//-----------------------------------------------------------------------------
// Helpers
//-----------------------------------------------------------------------------

const ruleValidator = new RuleValidator();

/**
 * Splits a plugin identifier in the form a/b/c into two parts: a/b and c.
 * @param {string} identifier The identifier to parse.
 * @returns {{objectName: string, pluginName: string}} The parts of the plugin
 *      name.
 */
function splitPluginIdentifier(identifier) {
    const parts = identifier.split("/");

    return {
        objectName: parts.pop(),
        pluginName: parts.join("/")
    };
}

/**
 * Returns the name of an object in the config by reading its `meta` key.
 * @param {Object} object The object to check.
 * @returns {string?} The name of the object if found or `null` if there
 *      is no name.
 */
function getObjectId(object) {

    // first check old-style name
    let name = object.name;

    if (!name) {

        if (!object.meta) {
            return null;
        }

        name = object.meta.name;

        if (!name) {
            return null;
        }
    }

    // now check for old-style version
    let version = object.version;

    if (!version) {
        version = object.meta && object.meta.version;
    }

    // if there's a version then append that
    if (version) {
        return `${name}@${version}`;
    }

    return name;
}

const originalBaseConfig = Symbol("originalBaseConfig");

//-----------------------------------------------------------------------------
// Exports
//-----------------------------------------------------------------------------

/**
 * Represents an array containing configuration information for ESLint.
 */
class FlatConfigArray extends ConfigArray {

    /**
     * Creates a new instance.
     * @param {*[]} configs An array of configuration information.
     * @param {{basePath: string, shouldIgnore: boolean, baseConfig: FlatConfig}} options The options
     *      to use for the config array instance.
     */
    constructor(configs, {
        basePath,
        shouldIgnore = true,
        baseConfig = defaultConfig
    } = {}) {
        super(configs, {
            basePath,
            schema: flatConfigSchema
        });

        if (baseConfig[Symbol.iterator]) {
            this.unshift(...baseConfig);
        } else {
            this.unshift(baseConfig);
        }

        /**
         * The base config used to build the config array.
         * @type {Array<FlatConfig>}
         */
        this[originalBaseConfig] = baseConfig;
        Object.defineProperty(this, originalBaseConfig, { writable: false });

        /**
         * Determines if `ignores` fields should be honored.
         * If true, then all `ignores` fields are honored.
         * if false, then only `ignores` fields in the baseConfig are honored.
         * @type {boolean}
         */
        this.shouldIgnore = shouldIgnore;
        Object.defineProperty(this, "shouldIgnore", { writable: false });
    }

    /* eslint-disable class-methods-use-this -- Desired as instance method */
    /**
     * Replaces a config with another config to allow us to put strings
     * in the config array that will be replaced by objects before
     * normalization.
     * @param {Object} config The config to preprocess.
     * @returns {Object} The preprocessed config.
     */
    [ConfigArraySymbol.preprocessConfig](config) {
        if (config === "eslint:recommended") {

            // if we are in a Node.js environment warn the user
            if (typeof process !== "undefined" && process.emitWarning) {
                process.emitWarning("The 'eslint:recommended' string configuration is deprecated and will be replaced by the @eslint/js package's 'recommended' config.");
            }

            return jsPlugin.configs.recommended;
        }

        if (config === "eslint:all") {

            // if we are in a Node.js environment warn the user
            if (typeof process !== "undefined" && process.emitWarning) {
                process.emitWarning("The 'eslint:all' string configuration is deprecated and will be replaced by the @eslint/js package's 'all' config.");
            }

            return jsPlugin.configs.all;
        }

        /*
         * If `shouldIgnore` is false, we remove any ignore patterns specified
         * in the config so long as it's not a default config and it doesn't
         * have a `files` entry.
         */
        if (
            !this.shouldIgnore &&
            !this[originalBaseConfig].includes(config) &&
            config.ignores &&
            !config.files
        ) {
            /* eslint-disable-next-line no-unused-vars -- need to strip off other keys */
            const { ignores, ...otherKeys } = config;

            return otherKeys;
        }

        return config;
    }

    /**
     * Finalizes the config by replacing plugin references with their objects
     * and validating rule option schemas.
     * @param {Object} config The config to finalize.
     * @returns {Object} The finalized config.
     * @throws {TypeError} If the config is invalid.
     */
    [ConfigArraySymbol.finalizeConfig](config) {

        const { plugins, languageOptions, processor } = config;
        let parserName, processorName;
        let invalidParser = false,
            invalidProcessor = false;

        // Check parser value
        if (languageOptions && languageOptions.parser) {
            const { parser } = languageOptions;

            if (typeof parser === "object") {
                parserName = getObjectId(parser);

                if (!parserName) {
                    invalidParser = true;
                }

            } else {
                invalidParser = true;
            }
        }

        // Check processor value
        if (processor) {
            if (typeof processor === "string") {
                const { pluginName, objectName: localProcessorName } = splitPluginIdentifier(processor);

                processorName = processor;

                if (!plugins || !plugins[pluginName] || !plugins[pluginName].processors || !plugins[pluginName].processors[localProcessorName]) {
                    throw new TypeError(`Key "processor": Could not find "${localProcessorName}" in plugin "${pluginName}".`);
                }

                config.processor = plugins[pluginName].processors[localProcessorName];
            } else if (typeof processor === "object") {
                processorName = getObjectId(processor);

                if (!processorName) {
                    invalidProcessor = true;
                }

            } else {
                invalidProcessor = true;
            }
        }

        ruleValidator.validate(config);

        // apply special logic for serialization into JSON
        /* eslint-disable object-shorthand -- shorthand would change "this" value */
        Object.defineProperty(config, "toJSON", {
            value: function() {

                if (invalidParser) {
                    throw new Error("Could not serialize parser object (missing 'meta' object).");
                }

                if (invalidProcessor) {
                    throw new Error("Could not serialize processor object (missing 'meta' object).");
                }

                return {
                    ...this,
                    plugins: Object.entries(plugins).map(([namespace, plugin]) => {

                        const pluginId = getObjectId(plugin);

                        if (!pluginId) {
                            return namespace;
                        }

                        return `${namespace}:${pluginId}`;
                    }),
                    languageOptions: {
                        ...languageOptions,
                        parser: parserName
                    },
                    processor: processorName
                };
            }
        });
        /* eslint-enable object-shorthand -- ok to enable now */

        return config;
    }
    /* eslint-enable class-methods-use-this -- Desired as instance method */

}

exports.FlatConfigArray = FlatConfigArray;
