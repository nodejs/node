/**
 * @fileoverview Common utilities.
 */
"use strict";

//------------------------------------------------------------------------------
// Constants
//------------------------------------------------------------------------------

var PLUGIN_NAME_PREFIX = "eslint-plugin-",
    NAMESPACE_REGEX = /^@.*\//i;

//------------------------------------------------------------------------------
// Public Interface
//------------------------------------------------------------------------------


/**
 * Removes the prefix `eslint-plugin-` from a plugin name.
 * @param {string} pluginName The name of the plugin which may have the prefix.
 * @returns {string} The name of the plugin without prefix.
 */
function removePluginPrefix(pluginName) {
    return pluginName.indexOf(PLUGIN_NAME_PREFIX) === 0 ? pluginName.substring(PLUGIN_NAME_PREFIX.length) : pluginName;
}

/**
 * @param {string} pluginName The name of the plugin which may have the prefix.
 * @returns {string} The name of the plugins namepace if it has one.
 */
function getNamespace(pluginName) {
    return pluginName.match(NAMESPACE_REGEX) ? pluginName.match(NAMESPACE_REGEX)[0] : "";
}

/**
 * Removes the namespace from a plugin name.
 * @param {string} pluginName The name of the plugin which may have the prefix.
 * @returns {string} The name of the plugin without the namespace.
 */
function removeNameSpace(pluginName) {
    return pluginName.replace(NAMESPACE_REGEX, "");
}

module.exports = {
    removePluginPrefix: removePluginPrefix,
    getNamespace: getNamespace,
    removeNameSpace: removeNameSpace,
    "PLUGIN_NAME_PREFIX": PLUGIN_NAME_PREFIX
};
