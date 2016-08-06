/**
 * @fileoverview Validates configs.
 * @author Brandon Mills
 */

"use strict";

//------------------------------------------------------------------------------
// Requirements
//------------------------------------------------------------------------------

let rules = require("../rules"),
    Environments = require("./environments"),
    schemaValidator = require("is-my-json-valid"),
    util = require("util");

let validators = {
    rules: Object.create(null)
};

//------------------------------------------------------------------------------
// Private
//------------------------------------------------------------------------------

/**
 * Gets a complete options schema for a rule.
 * @param {string} id The rule's unique name.
 * @returns {Object} JSON Schema for the rule's options.
 */
function getRuleOptionsSchema(id) {
    let rule = rules.get(id),
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
        } else {
            return {
                type: "array",
                minItems: 0,
                maxItems: 0
            };
        }
    }

    // Given a full schema, leave it alone
    return schema || null;
}

/**
 * Validates a rule's options against its schema.
 * @param {string} id The rule's unique name.
 * @param {array|number} options The given options for the rule.
 * @param {string} source The name of the configuration source.
 * @returns {void}
 */
function validateRuleOptions(id, options, source) {
    let validateRule = validators.rules[id],
        message,
        severity,
        localOptions,
        schema = getRuleOptionsSchema(id),
        validSeverity = true;

    if (!validateRule && schema) {
        validateRule = schemaValidator(schema, { verbose: true });
        validators.rules[id] = validateRule;
    }

    // if it's not an array, it should be just a severity
    if (Array.isArray(options)) {
        localOptions = options.concat();    // clone
        severity = localOptions.shift();
    } else {
        severity = options;
        localOptions = [];
    }

    validSeverity = (
        severity === 0 || severity === 1 || severity === 2 ||
        (typeof severity === "string" && /^(?:off|warn|error)$/i.test(severity))
    );

    if (validateRule) {
        validateRule(localOptions);
    }

    if ((validateRule && validateRule.errors) || !validSeverity) {
        message = [
            source, ":\n",
            "\tConfiguration for rule \"", id, "\" is invalid:\n"
        ];

        if (!validSeverity) {
            message.push(
                "\tSeverity should be one of the following: 0 = off, 1 = warn, 2 = error (you passed '",
                util.inspect(severity).replace(/'/g, "\"").replace(/\n/g, ""),
                "').\n"
            );
        }

        if (validateRule && validateRule.errors) {
            validateRule.errors.forEach(function(error) {
                message.push(
                    "\tValue \"", error.value, "\" ", error.message, ".\n"
                );
            });
        }

        throw new Error(message.join(""));
    }
}

/**
 * Validates an environment object
 * @param {Object} environment The environment config object to validate.
 * @param {string} source The location to report with any errors.
 * @returns {void}
 */
function validateEnvironment(environment, source) {

    // not having an environment is ok
    if (!environment) {
        return;
    }

    if (Array.isArray(environment)) {
        throw new Error("Environment must not be an array");
    }

    if (typeof environment === "object") {
        Object.keys(environment).forEach(function(env) {
            if (!Environments.get(env)) {
                let message = [
                    source, ":\n",
                    "\tEnvironment key \"", env, "\" is unknown\n"
                ];

                throw new Error(message.join(""));
            }
        });
    } else {
        throw new Error("Environment must be an object");
    }
}

/**
 * Validates an entire config object.
 * @param {Object} config The config object to validate.
 * @param {string} source The location to report with any errors.
 * @returns {void}
 */
function validate(config, source) {

    if (typeof config.rules === "object") {
        Object.keys(config.rules).forEach(function(id) {
            validateRuleOptions(id, config.rules[id], source);
        });
    }

    validateEnvironment(config.env, source);
}

//------------------------------------------------------------------------------
// Public Interface
//------------------------------------------------------------------------------

module.exports = {
    getRuleOptionsSchema: getRuleOptionsSchema,
    validate: validate,
    validateRuleOptions: validateRuleOptions
};
