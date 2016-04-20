/**
 * @fileoverview Defines a storage for rules.
 * @author Nicholas C. Zakas
 */

"use strict";

//------------------------------------------------------------------------------
// Requirements
//------------------------------------------------------------------------------

var loadRules = require("./load-rules");

//------------------------------------------------------------------------------
// Privates
//------------------------------------------------------------------------------

var rules = Object.create(null);

//------------------------------------------------------------------------------
// Public Interface
//------------------------------------------------------------------------------

/**
 * Registers a rule module for rule id in storage.
 * @param {string} ruleId Rule id (file name).
 * @param {Function} ruleModule Rule handler.
 * @returns {void}
 */
function define(ruleId, ruleModule) {
    rules[ruleId] = ruleModule;
}

/**
 * Loads and registers all rules from passed rules directory.
 * @param {string} [rulesDir] Path to rules directory, may be relative. Defaults to `lib/rules`.
 * @param {string} cwd Current working directory
 * @returns {void}
 */
function load(rulesDir, cwd) {
    var newRules = loadRules(rulesDir, cwd);

    Object.keys(newRules).forEach(function(ruleId) {
        define(ruleId, newRules[ruleId]);
    });
}

/**
 * Registers all given rules of a plugin.
 * @param {Object} pluginRules A key/value map of rule definitions.
 * @param {string} pluginName The name of the plugin without prefix (`eslint-plugin-`).
 * @returns {void}
 */
function importPlugin(pluginRules, pluginName) {
    Object.keys(pluginRules).forEach(function(ruleId) {
        var qualifiedRuleId = pluginName + "/" + ruleId,
            rule = pluginRules[ruleId];

        define(qualifiedRuleId, rule);
    });
}

/**
 * Access rule handler by id (file name).
 * @param {string} ruleId Rule id (file name).
 * @returns {Function} Rule handler.
 */
function get(ruleId) {
    if (typeof rules[ruleId] === "string") {
        return require(rules[ruleId]);
    } else {
        return rules[ruleId];
    }
}

/**
 * Reset rules storage.
 * Should be used only in tests.
 * @returns {void}
 */
function testClear() {
    rules = Object.create(null);
}

module.exports = {
    define: define,
    load: load,
    import: importPlugin,
    get: get,
    testClear: testClear,

    /**
     * Resets rules to its starting state. Use for tests only.
     * @returns {void}
     */
    testReset: function() {
        testClear();
        load();
    }
};

//------------------------------------------------------------------------------
// Initialization
//------------------------------------------------------------------------------

// loads built-in rules
load();
