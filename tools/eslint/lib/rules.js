/**
 * @fileoverview Defines a storage for rules.
 * @author Nicholas C. Zakas
 */

"use strict";

//------------------------------------------------------------------------------
// Requirements
//------------------------------------------------------------------------------

const loadRules = require("./load-rules");

//------------------------------------------------------------------------------
// Public Interface
//------------------------------------------------------------------------------

class Rules {
    constructor() {
        this._rules = Object.create(null);

        this.load();
    }

    /**
     * Registers a rule module for rule id in storage.
     * @param {string} ruleId Rule id (file name).
     * @param {Function} ruleModule Rule handler.
     * @returns {void}
     */
    define(ruleId, ruleModule) {
        this._rules[ruleId] = ruleModule;
    }

    /**
     * Loads and registers all rules from passed rules directory.
     * @param {string} [rulesDir] Path to rules directory, may be relative. Defaults to `lib/rules`.
     * @param {string} cwd Current working directory
     * @returns {void}
     */
    load(rulesDir, cwd) {
        const newRules = loadRules(rulesDir, cwd);

        Object.keys(newRules).forEach(ruleId => {
            this.define(ruleId, newRules[ruleId]);
        });
    }

    /**
     * Registers all given rules of a plugin.
     * @param {Object} plugin The plugin object to import.
     * @param {string} pluginName The name of the plugin without prefix (`eslint-plugin-`).
     * @returns {void}
     */
    importPlugin(plugin, pluginName) {
        if (plugin.rules) {
            Object.keys(plugin.rules).forEach(ruleId => {
                const qualifiedRuleId = `${pluginName}/${ruleId}`,
                    rule = plugin.rules[ruleId];

                this.define(qualifiedRuleId, rule);
            });
        }
    }

    /**
     * Access rule handler by id (file name).
     * @param {string} ruleId Rule id (file name).
     * @returns {Function} Rule handler.
     */
    get(ruleId) {
        if (typeof this._rules[ruleId] === "string") {
            return require(this._rules[ruleId]);
        }
        return this._rules[ruleId];

    }

    /**
     * Get an object with all currently loaded rules
     * @returns {Map} All loaded rules
     */
    getAllLoadedRules() {
        const allRules = new Map();

        Object.keys(this._rules).forEach(name => {
            const rule = this.get(name);

            allRules.set(name, rule);
        });
        return allRules;
    }
}

module.exports = Rules;
