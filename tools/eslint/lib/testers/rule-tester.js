/**
 * @fileoverview Mocha test wrapper
 * @author Ilya Volodin
 */
"use strict";

/* global describe, it */

/*
 * This is a wrapper around mocha to allow for DRY unittests for eslint
 * Format:
 * RuleTester.add("{ruleName}", {
 *      valid: [
 *          "{code}",
 *          { code: "{code}", options: {options}, global: {globals}, globals: {globals}, parser: "{parser}", settings: {settings} }
 *      ],
 *      invalid: [
 *          { code: "{code}", errors: {numErrors} },
 *          { code: "{code}", errors: ["{errorMessage}"] },
 *          { code: "{code}", options: {options}, global: {globals}, parser: "{parser}", settings: {settings}, errors: [{ message: "{errorMessage}", type: "{errorNodeType}"}] }
 *      ]
 *  });
 *
 * Variables:
 * {code} - String that represents the code to be tested
 * {options} - Arguments that are passed to the configurable rules.
 * {globals} - An object representing a list of variables that are
 *             registered as globals
 * {parser} - String representing the parser to use
 * {settings} - An object representing global settings for all rules
 * {numErrors} - If failing case doesn't need to check error message,
 *               this integer will specify how many errors should be
 *               received
 * {errorMessage} - Message that is returned by the rule on failure
 * {errorNodeType} - AST node type that is returned by they rule as
 *                   a cause of the failure.
 */

//------------------------------------------------------------------------------
// Requirements
//------------------------------------------------------------------------------

const lodash = require("lodash"),
    assert = require("assert"),
    util = require("util"),
    validator = require("../config/config-validator"),
    validate = require("is-my-json-valid"),
    eslint = require("../eslint"),
    rules = require("../rules"),
    metaSchema = require("../../conf/json-schema-schema.json"),
    SourceCodeFixer = require("../util/source-code-fixer");

//------------------------------------------------------------------------------
// Private Members
//------------------------------------------------------------------------------

/*
 * testerDefaultConfig must not be modified as it allows to reset the tester to
 * the initial default configuration
 */
const testerDefaultConfig = { rules: {} };
let defaultConfig = { rules: {} };

/*
 * List every parameters possible on a test case that are not related to eslint
 * configuration
 */
const RuleTesterParameters = [
    "code",
    "filename",
    "options",
    "args",
    "errors"
];

const validateSchema = validate(metaSchema, { verbose: true });

const hasOwnProperty = Function.call.bind(Object.hasOwnProperty);

/**
 * Clones a given value deeply.
 * Note: This ignores `parent` property.
 *
 * @param {any} x - A value to clone.
 * @returns {any} A cloned value.
 */
function cloneDeeplyExcludesParent(x) {
    if (typeof x === "object" && x !== null) {
        if (Array.isArray(x)) {
            return x.map(cloneDeeplyExcludesParent);
        }

        const retv = {};

        for (const key in x) {
            if (key !== "parent" && hasOwnProperty(x, key)) {
                retv[key] = cloneDeeplyExcludesParent(x[key]);
            }
        }

        return retv;
    }

    return x;
}

/**
 * Freezes a given value deeply.
 *
 * @param {any} x - A value to freeze.
 * @returns {void}
 */
function freezeDeeply(x) {
    if (typeof x === "object" && x !== null) {
        if (Array.isArray(x)) {
            x.forEach(freezeDeeply);
        } else {
            for (const key in x) {
                if (key !== "parent" && hasOwnProperty(x, key)) {
                    freezeDeeply(x[key]);
                }
            }
        }
        Object.freeze(x);
    }
}

//------------------------------------------------------------------------------
// Public Interface
//------------------------------------------------------------------------------

/**
 * Creates a new instance of RuleTester.
 * @param {Object} [testerConfig] Optional, extra configuration for the tester
 * @constructor
 */
function RuleTester(testerConfig) {

    /**
     * The configuration to use for this tester. Combination of the tester
     * configuration and the default configuration.
     * @type {Object}
     */
    this.testerConfig = lodash.merge(

        // we have to clone because merge uses the first argument for recipient
        lodash.cloneDeep(defaultConfig),
        testerConfig
    );
}

/**
 * Set the configuration to use for all future tests
 * @param {Object} config the configuration to use.
 * @returns {void}
 */
RuleTester.setDefaultConfig = function(config) {
    if (typeof config !== "object") {
        throw new Error("RuleTester.setDefaultConfig: config must be an object");
    }
    defaultConfig = config;

    // Make sure the rules object exists since it is assumed to exist later
    defaultConfig.rules = defaultConfig.rules || {};
};

/**
 * Get the current configuration used for all tests
 * @returns {Object} the current configuration
 */
RuleTester.getDefaultConfig = function() {
    return defaultConfig;
};

/**
 * Reset the configuration to the initial configuration of the tester removing
 * any changes made until now.
 * @returns {void}
 */
RuleTester.resetDefaultConfig = function() {
    defaultConfig = lodash.cloneDeep(testerDefaultConfig);
};

// default separators for testing
RuleTester.describe = (typeof describe === "function") ? describe : /* istanbul ignore next */ function(text, method) {
    return method.apply(this);
};

RuleTester.it = (typeof it === "function") ? it : /* istanbul ignore next */ function(text, method) {
    return method.apply(this);
};

RuleTester.prototype = {

    /**
     * Define a rule for one particular run of tests.
     * @param {string} name The name of the rule to define.
     * @param {Function} rule The rule definition.
     * @returns {void}
     */
    defineRule(name, rule) {
        eslint.defineRule(name, rule);
    },

    /**
     * Adds a new rule test to execute.
     * @param {string} ruleName The name of the rule to run.
     * @param {Function} rule The rule to test.
     * @param {Object} test The collection of tests to run.
     * @returns {void}
     */
    run(ruleName, rule, test) {

        const testerConfig = this.testerConfig,
            result = {};

        /* eslint-disable no-shadow */

        /**
         * Run the rule for the given item
         * @param {string} ruleName name of the rule
         * @param {string|Object} item Item to run the rule against
         * @returns {Object} Eslint run result
         * @private
         */
        function runRuleForItem(ruleName, item) {
            let config = lodash.cloneDeep(testerConfig),
                code, filename, beforeAST, afterAST;

            if (typeof item === "string") {
                code = item;
            } else {
                code = item.code;

                // Assumes everything on the item is a config except for the
                // parameters used by this tester
                const itemConfig = lodash.omit(item, RuleTesterParameters);

                // Create the config object from the tester config and this item
                // specific configurations.
                config = lodash.merge(
                    config,
                    itemConfig
                );
            }

            if (item.filename) {
                filename = item.filename;
            }

            if (item.options) {
                const options = item.options.concat();

                options.unshift(1);
                config.rules[ruleName] = options;
            } else {
                config.rules[ruleName] = 1;
            }

            eslint.defineRule(ruleName, rule);

            const schema = validator.getRuleOptionsSchema(ruleName);

            if (schema) {
                validateSchema(schema);

                if (validateSchema.errors) {
                    throw new Error([
                        "Schema for rule " + ruleName + " is invalid:"
                    ].concat(validateSchema.errors.map(function(error) {
                        return "\t" + error.field + ": " + error.message;
                    })).join("\n"));
                }
            }

            validator.validate(config, "rule-tester");

            /*
             * Setup AST getters.
             * The goal is to check whether or not AST was modified when
             * running the rule under test.
             */
            eslint.reset();
            eslint.on("Program", function(node) {
                beforeAST = cloneDeeplyExcludesParent(node);

                eslint.on("Program:exit", function(node) {
                    afterAST = cloneDeeplyExcludesParent(node);
                });
            });

            // Freezes rule-context properties.
            const originalGet = rules.get;

            try {
                rules.get = function(ruleId) {
                    const rule = originalGet(ruleId);

                    if (typeof rule === "function") {
                        return function(context) {
                            Object.freeze(context);
                            freezeDeeply(context.options);
                            freezeDeeply(context.settings);
                            freezeDeeply(context.parserOptions);

                            return rule(context);
                        };
                    } else {
                        return {
                            meta: rule.meta,
                            create(context) {
                                Object.freeze(context);
                                freezeDeeply(context.options);
                                freezeDeeply(context.settings);
                                freezeDeeply(context.parserOptions);

                                return rule.create(context);
                            }
                        };
                    }
                };

                return {
                    messages: eslint.verify(code, config, filename, true),
                    beforeAST,
                    afterAST
                };
            } finally {
                rules.get = originalGet;
            }
        }

        /**
         * Check if the AST was changed
         * @param {ASTNode} beforeAST AST node before running
         * @param {ASTNode} afterAST AST node after running
         * @returns {void}
         * @private
         */
        function assertASTDidntChange(beforeAST, afterAST) {
            if (!lodash.isEqual(beforeAST, afterAST)) {

                // Not using directly to avoid performance problem in node 6.1.0. See #6111
                assert.deepEqual(beforeAST, afterAST, "Rule should not modify AST.");
            }
        }

        /**
         * Check if the template is valid or not
         * all valid cases go through this
         * @param {string} ruleName name of the rule
         * @param {string|Object} item Item to run the rule against
         * @returns {void}
         * @private
         */
        function testValidTemplate(ruleName, item) {
            const result = runRuleForItem(ruleName, item);
            const messages = result.messages;

            assert.equal(messages.length, 0, util.format("Should have no errors but had %d: %s",
                        messages.length, util.inspect(messages)));

            assertASTDidntChange(result.beforeAST, result.afterAST);
        }

        /**
         * Check if the template is invalid or not
         * all invalid cases go through this.
         * @param {string} ruleName name of the rule
         * @param {string|Object} item Item to run the rule against
         * @returns {void}
         * @private
         */
        function testInvalidTemplate(ruleName, item) {
            assert.ok(item.errors || item.errors === 0,
                "Did not specify errors for an invalid test of " + ruleName);

            const result = runRuleForItem(ruleName, item);
            const messages = result.messages;



            if (typeof item.errors === "number") {
                assert.equal(messages.length, item.errors, util.format("Should have %d error%s but had %d: %s",
                    item.errors, item.errors === 1 ? "" : "s", messages.length, util.inspect(messages)));
            } else {
                assert.equal(messages.length, item.errors.length,
                    util.format("Should have %d error%s but had %d: %s",
                    item.errors.length, item.errors.length === 1 ? "" : "s", messages.length, util.inspect(messages)));

                for (let i = 0, l = item.errors.length; i < l; i++) {
                    assert.ok(!("fatal" in messages[i]), "A fatal parsing error occurred: " + messages[i].message);
                    assert.equal(messages[i].ruleId, ruleName, "Error rule name should be the same as the name of the rule being tested");

                    if (typeof item.errors[i] === "string") {

                        // Just an error message.
                        assert.equal(messages[i].message, item.errors[i]);
                    } else if (typeof item.errors[i] === "object") {

                        /*
                         * Error object.
                         * This may have a message, node type, line, and/or
                         * column.
                         */
                        if (item.errors[i].message) {
                            assert.equal(messages[i].message, item.errors[i].message);
                        }

                        if (item.errors[i].type) {
                            assert.equal(messages[i].nodeType, item.errors[i].type, "Error type should be " + item.errors[i].type);
                        }

                        if (item.errors[i].hasOwnProperty("line")) {
                            assert.equal(messages[i].line, item.errors[i].line, "Error line should be " + item.errors[i].line);
                        }

                        if (item.errors[i].hasOwnProperty("column")) {
                            assert.equal(messages[i].column, item.errors[i].column, "Error column should be " + item.errors[i].column);
                        }

                        if (item.errors[i].hasOwnProperty("endLine")) {
                            assert.equal(messages[i].endLine, item.errors[i].endLine, "Error endLine should be " + item.errors[i].endLine);
                        }

                        if (item.errors[i].hasOwnProperty("endColumn")) {
                            assert.equal(messages[i].endColumn, item.errors[i].endColumn, "Error endColumn should be " + item.errors[i].endColumn);
                        }
                    } else {

                        // Only string or object errors are valid.
                        assert.fail(messages[i], null, "Error should be a string or object.");
                    }
                }

                if (item.hasOwnProperty("output")) {
                    const fixResult = SourceCodeFixer.applyFixes(eslint.getSourceCode(), messages);

                    assert.equal(fixResult.output, item.output, "Output is incorrect.");
                }

            }

            assertASTDidntChange(result.beforeAST, result.afterAST);
        }

        /*
         * This creates a mocha test suite and pipes all supplied info through
         * one of the templates above.
         */
        RuleTester.describe(ruleName, function() {
            RuleTester.describe("valid", function() {
                test.valid.forEach(function(valid) {
                    RuleTester.it(valid.code || valid, function() {
                        testValidTemplate(ruleName, valid);
                    });
                });
            });

            RuleTester.describe("invalid", function() {
                test.invalid.forEach(function(invalid) {
                    RuleTester.it(invalid.code, function() {
                        testInvalidTemplate(ruleName, invalid);
                    });
                });
            });
        });

        return result.suite;
    }
};


module.exports = RuleTester;
