/**
 * @fileoverview Environments manager
 * @author Nicholas C. Zakas
 */
"use strict";

//------------------------------------------------------------------------------
// Requirements
//------------------------------------------------------------------------------

var debug = require("debug"),
    envs = require("../../conf/environments");

//------------------------------------------------------------------------------
// Private
//------------------------------------------------------------------------------

debug = debug("eslint:enviroments");

var environments = Object.create(null);

/**
 * Loads the default environments.
 * @returns {void}
 * @private
 */
function load() {
    Object.keys(envs).forEach(function(envName) {
        environments[envName] = envs[envName];
    });
}

// always load default environments upfront
load();

//------------------------------------------------------------------------------
// Public Interface
//------------------------------------------------------------------------------

module.exports = {

    load: load,

    /**
     * Gets the environment with the given name.
     * @param {string} name The name of the environment to retrieve.
     * @returns {Object?} The environment object or null if not found.
     */
    get: function(name) {
        return environments[name] || null;
    },

    /**
     * Defines an environment.
     * @param {string} name The name of the environment.
     * @param {Object} env The environment settings.
     * @returns {void}
     */
    define: function(name, env) {
        environments[name] = env;
    },

    /**
     * Imports all environments from a plugin.
     * @param {Object} plugin The plugin object.
     * @param {string} pluginName The name of the plugin.
     * @returns {void}
     */
    importPlugin: function(plugin, pluginName) {
        if (plugin.environments) {
            Object.keys(plugin.environments).forEach(function(envName) {
                this.define(pluginName + "/" + envName, plugin.environments[envName]);
            }, this);
        }
    },

    /**
     * Resets all environments. Only use for tests!
     * @returns {void}
     */
    testReset: function() {
        environments = Object.create(null);
        load();
    }
};
