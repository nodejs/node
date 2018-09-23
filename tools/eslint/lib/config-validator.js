/**
 * @fileoverview Validates configs.
 * @author Brandon Mills
 * @copyright 2015 Brandon Mills
 */

"use strict";

var rules = require("./rules"),
    schemaValidator = require("is-my-json-valid");

var validators = {
    rules: Object.create(null)
};

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
        validateRule.errors.forEach(function (error) {
            message.push(
                "\tValue \"", error.value, "\" ", error.message, ".\n"
            );
        });

        throw new Error(message.join(""));
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
        Object.keys(config.rules).forEach(function (id) {
            validateRuleOptions(id, config.rules[id], source);
        });
    }
}

module.exports = {
    getRuleOptionsSchema: getRuleOptionsSchema,
    validate: validate,
    validateRuleOptions: validateRuleOptions
};
