/**
 * @fileoverview Validates configs.
 * @author Brandon Mills
 * @copyright 2015 Brandon Mills
 */

"use strict";

//------------------------------------------------------------------------------
// Requirements
//------------------------------------------------------------------------------

var rules = require("../rules"),
    environments = require("../../conf/environments"),
    schemaValidator = require("is-my-json-valid");

var validators = {
    rules: Object.create(null)
};

//------------------------------------------------------------------------------
// Private
//------------------------------------------------------------------------------

/**
 * Gets a complete options schema for a rule.
 * @param {string} id The rule's unique name.
 * @returns {object} JSON Schema for the rule's options.
 */
function getRuleOptionsSchema(id) {
    var rule = rules.get(id),
        schema = rule && rule.schema;

    if (!schema) {
        return {
            "type": "array",
            "items": [
                {
                    "enum": [0, 1, 2]
                }
            ],
            "minItems": 1
        };
    }

    // Given a tuple of schemas, insert warning level at the beginning
    if (Array.isArray(schema)) {
        return {
            "type": "array",
            "items": [
                {
                    "enum": [0, 1, 2]
                }
            ].concat(schema),
            "minItems": 1,
            "maxItems": schema.length + 1
        };
    }

    // Given a full schema, leave it alone
    return schema;
}

/**
 * Validates a rule's options against its schema.
 * @param {string} id The rule's unique name.
 * @param {array|number} options The given options for the rule.
 * @param {string} source The name of the configuration source.
 * @returns {void}
 */
function validateRuleOptions(id, options, source) {
    var validateRule = validators.rules[id],
        message;

    if (!validateRule) {
        validateRule = schemaValidator(getRuleOptionsSchema(id), { verbose: true });
        validators.rules[id] = validateRule;
    }

    if (typeof options === "number") {
        options = [options];
    }

    validateRule(options);

    if (validateRule.errors) {
        message = [
            source, ":\n",
            "\tConfiguration for rule \"", id, "\" is invalid:\n"
        ];
        validateRule.errors.forEach(function(error) {
            if (error.field === "data[\"0\"]") {  // better error for severity
                message.push(
                    "\tSeverity should be one of the following: 0 = off, 1 = warning, 2 = error (you passed \"", error.value, "\").\n");
            } else {
                message.push(
                    "\tValue \"", error.value, "\" ", error.message, ".\n"
                );
            }
        });

        throw new Error(message.join(""));
    }
}

/**
 * Validates an environment object
 * @param {object} environment The environment config object to validate.
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
            if (!environments[env]) {
                var message = [
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
 * @param {object} config The config object to validate.
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
