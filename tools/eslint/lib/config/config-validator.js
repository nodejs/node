/**
 * @fileoverview Validates configs.
 * @author Brandon Mills
 */

"use strict";

//------------------------------------------------------------------------------
// Requirements
//------------------------------------------------------------------------------

const schemaValidator = require("is-my-json-valid"),
    configSchema = require("../../conf/config-schema.json"),
    util = require("util");

const validators = {
    rules: Object.create(null)
};

//------------------------------------------------------------------------------
// Private
//------------------------------------------------------------------------------

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
        validators.rules[id] = schemaValidator(schema, { verbose: true });
    }

    const validateRule = validators.rules[id];

    if (validateRule) {
        validateRule(localOptions);
        if (validateRule.errors) {
            throw new Error(validateRule.errors.map(error => `\tValue "${error.value}" ${error.message}.\n`).join(""));
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
        if (error.message === "has additional properties") {
            return `Unexpected top-level property "${error.value.replace(/^data\./, "")}"`;
        }
        if (error.message === "is the wrong type") {
            const formattedField = error.field.replace(/^data\./, "");
            const formattedExpectedType = typeof error.type === "string" ? error.type : error.type.join("/");
            const formattedValue = JSON.stringify(error.value);

            return `Property "${formattedField}" is the wrong type (expected ${formattedExpectedType} but got \`${formattedValue}\`)`;
        }
        return `"${error.field.replace(/^(data\.)/, "")}" ${error.message}. Value: ${error.value}`;
    }).map(message => `\t- ${message}.\n`).join("");
}

/**
 * Validates the top level properties of the config object.
 * @param {Object} config The config object to validate.
 * @param {string} source The name of the configuration source to report in any errors.
 * @returns {void}
 */
function validateConfigSchema(config, source) {
    const validator = schemaValidator(configSchema, { verbose: true });

    if (!validator(config)) {
        throw new Error(`${source}:\n\tESLint configuration is invalid:\n${formatErrors(validator.errors)}`);
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
