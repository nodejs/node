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
const recommendedConfig = require("../../conf/eslint-recommended");

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
         * The baes config used to build the config array.
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
            return recommendedConfig;
        }

        if (config === "eslint:all") {

            /*
             * Load `eslint-all.js` here instead of at the top level to avoid loading all rule modules
             * when it isn't necessary. `eslint-all.js` reads `meta` of rule objects to filter out deprecated ones,
             * so requiring `eslint-all.js` module loads all rule modules as a consequence.
             */
            return require("../../conf/eslint-all");
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

        // Check parser value
        if (languageOptions && languageOptions.parser && typeof languageOptions.parser === "string") {
            const { pluginName, objectName: parserName } = splitPluginIdentifier(languageOptions.parser);

            if (!plugins || !plugins[pluginName] || !plugins[pluginName].parsers || !plugins[pluginName].parsers[parserName]) {
                throw new TypeError(`Key "parser": Could not find "${parserName}" in plugin "${pluginName}".`);
            }

            languageOptions.parser = plugins[pluginName].parsers[parserName];
        }

        // Check processor value
        if (processor && typeof processor === "string") {
            const { pluginName, objectName: processorName } = splitPluginIdentifier(processor);

            if (!plugins || !plugins[pluginName] || !plugins[pluginName].processors || !plugins[pluginName].processors[processorName]) {
                throw new TypeError(`Key "processor": Could not find "${processorName}" in plugin "${pluginName}".`);
            }

            config.processor = plugins[pluginName].processors[processorName];
        }

        ruleValidator.validate(config);

        return config;
    }
    /* eslint-enable class-methods-use-this -- Desired as instance method */

}

exports.FlatConfigArray = FlatConfigArray;
