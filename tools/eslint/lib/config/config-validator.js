/**
 * @fileoverview Validates configs.
 * @author Brandon Mills
 */

"use strict";

//------------------------------------------------------------------------------
// Requirements
//------------------------------------------------------------------------------

const ajv = require("../util/ajv"),
    lodash = require("lodash"),
    configSchema = require("../../conf/config-schema.js"),
    util = require("util");

const validators = {
    rules: Object.create(null)
};

//------------------------------------------------------------------------------
// Private
//------------------------------------------------------------------------------
let validateSchema;

/**
 * Gets a complete options schema for a rule.
 * @param {string} id The rule's unique name.
 * @param {Rules} rulesContext Rule context
 * @returns {Object} JSON Schema for the rule's options.
 */
function getRuleOptionsSchema(id, rulesContext) {
    const rule = rulesContext.get(id),
        schema = rule && rule.schema || rule && rule.meta && rule.meta.schema;

    // Given a tuple of schemas, insert warning level at the beginning
    if (Array.isArray(schema)) {
        if (schema.length) {
            return {
                type: "array",
                items: schema,
                minItems: 0,
                maxItems: schema.length
            };
        }
        return {
            type: "array",
            minItems: 0,
            maxItems: 0
        };

    }

    // Given a full schema, leave it alone
    return schema || null;
}

/**
* Validates a rule's severity and returns the severity value. Throws an error if the severity is invalid.
* @param {options} options The given options for the rule.
* @returns {number|string} The rule's severity value
*/
function validateRuleSeverity(options) {
    const severity = Array.isArray(options) ? options[0] : options;

    if (severity !== 0 && severity !== 1 && severity !== 2 && !(typeof severity === "string" && /^(?:off|warn|error)$/i.test(severity))) {
        throw new Error(`\tSeverity should be one of the following: 0 = off, 1 = warn, 2 = error (you passed '${util.inspect(severity).replace(/'/g, "\"").replace(/\n/g, "")}').\n`);
    }

    return severity;
}

/**
* Validates the non-severity options passed to a rule, based on its schema.
* @param {string} id The rule's unique name
* @param {array} localOptions The options for the rule, excluding severity
* @param {Rules} rulesContext Rule context
* @returns {void}
*/
function validateRuleSchema(id, localOptions, rulesContext) {
    const schema = getRuleOptionsSchema(id, rulesContext);

    if (!validators.rules[id] && schema) {
        validators.rules[id] = ajv.compile(schema);
    }

    const validateRule = validators.rules[id];

    if (validateRule) {
        validateRule(localOptions);
        if (validateRule.errors) {
            throw new Error(validateRule.errors.map(error => `\tValue "${error.data}" ${error.message}.\n`).join(""));
        }
    }
}

/**
 * Validates a rule's options against its schema.
 * @param {string} id The rule's unique name.
 * @param {array|number} options The given options for the rule.
 * @param {string} source The name of the configuration source to report in any errors.
 * @param {Rules} rulesContext Rule context
 * @returns {void}
 */
function validateRuleOptions(id, options, source, rulesContext) {
    try {
        const severity = validateRuleSeverity(options);

        if (severity !== 0 && !(typeof severity === "string" && severity.toLowerCase() === "off")) {
            validateRuleSchema(id, Array.isArray(options) ? options.slice(1) : [], rulesContext);
        }
    } catch (err) {
        throw new Error(`${source}:\n\tConfiguration for rule "${id}" is invalid:\n${err.message}`);
    }
}

/**
 * Validates an environment object
 * @param {Object} environment The environment config object to validate.
 * @param {string} source The name of the configuration source to report in any errors.
 * @param {Environments} envContext Env context
 * @returns {void}
 */
function validateEnvironment(environment, source, envContext) {

    // not having an environment is ok
    if (!environment) {
        return;
    }

    Object.keys(environment).forEach(env => {
        if (!envContext.get(env)) {
            const message = `${source}:\n\tEnvironment key "${env}" is unknown\n`;

            throw new Error(message);
        }
    });
}

/**
 * Validates a rules config object
 * @param {Object} rulesConfig The rules config object to validate.
 * @param {string} source The name of the configuration source to report in any errors.
 * @param {Rules} rulesContext Rule context
 * @returns {void}
 */
function validateRules(rulesConfig, source, rulesContext) {
    if (!rulesConfig) {
        return;
    }

    Object.keys(rulesConfig).forEach(id => {
        validateRuleOptions(id, rulesConfig[id], source, rulesContext);
    });
}

/**
 * Formats an array of schema validation errors.
 * @param {Array} errors An array of error messages to format.
 * @returns {string} Formatted error message
 */
function formatErrors(errors) {
    return errors.map(error => {
        if (error.keyword === "additionalProperties") {
            const formattedPropertyPath = error.dataPath.length ? `${error.dataPath.slice(1)}.${error.params.additionalProperty}` : error.params.additionalProperty;

            return `Unexpected top-level property "${formattedPropertyPath}"`;
        }
        if (error.keyword === "type") {
            const formattedField = error.dataPath.slice(1);
            const formattedExpectedType = Array.isArray(error.schema) ? error.schema.join("/") : error.schema;
            const formattedValue = JSON.stringify(error.data);

            return `Property "${formattedField}" is the wrong type (expected ${formattedExpectedType} but got \`${formattedValue}\`)`;
        }

        const field = error.dataPath[0] === "." ? error.dataPath.slice(1) : error.dataPath;

        return `"${field}" ${error.message}. Value: ${JSON.stringify(error.data)}`;
    }).map(message => `\t- ${message}.\n`).join("");
}

/**
 * Emits a deprecation warning containing a given filepath. A new deprecation warning is emitted
 * for each unique file path, but repeated invocations with the same file path have no effect.
 * No warnings are emitted if the `--no-deprecation` or `--no-warnings` Node runtime flags are active.
 * @param {string} source The name of the configuration source to report the warning for.
 * @returns {void}
 */
const emitEcmaFeaturesWarning = lodash.memoize(source => {

    /*
     * util.deprecate seems to be the only way to emit a warning in Node 4.x while respecting the --no-warnings flag.
     * (In Node 6+, process.emitWarning could be used instead.)
     */
    util.deprecate(
        () => {},
        `[eslint] The 'ecmaFeatures' config file property is deprecated, and has no effect. (found in ${source})`
    )();
});

/**
 * Validates the top level properties of the config object.
 * @param {Object} config The config object to validate.
 * @param {string} source The name of the configuration source to report in any errors.
 * @returns {void}
 */
function validateConfigSchema(config, source) {
    validateSchema = validateSchema || ajv.compile(configSchema);

    if (!validateSchema(config)) {
        throw new Error(`ESLint configuration in ${source} is invalid:\n${formatErrors(validateSchema.errors)}`);
    }

    if (Object.prototype.hasOwnProperty.call(config, "ecmaFeatures")) {
        emitEcmaFeaturesWarning(source);
    }
}

/**
 * Validates an entire config object.
 * @param {Object} config The config object to validate.
 * @param {string} source The name of the configuration source to report in any errors.
 * @param {Rules} rulesContext The rules context
 * @param {Environments} envContext The env context
 * @returns {void}
 */
function validate(config, source, rulesContext, envContext) {
    validateConfigSchema(config, source);
    validateRules(config.rules, source, rulesContext);
    validateEnvironment(config.env, source, envContext);
}

//------------------------------------------------------------------------------
// Public Interface
//------------------------------------------------------------------------------

module.exports = {
    getRuleOptionsSchema,
    validate,
    validateRuleOptions
};
