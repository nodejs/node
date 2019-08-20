/**
 * Utility for resolving a module relative to another module
 * @author Teddy Katz
 */

"use strict";

const Module = require("module");
const path = require("path");

// Polyfill Node's `Module.createRequire` if not present. We only support the case where the argument is a filepath, not a URL.
const createRequire = (

    // Added in v12.2.0
    Module.createRequire ||

    // Added in v10.12.0, but deprecated in v12.2.0.
    Module.createRequireFromPath ||

    // Polyfill - This is not executed on the tests on node@>=10.
    /* istanbul ignore next */
    (filename => {
        const mod = new Module(filename, null);

        mod.filename = filename;
        mod.paths = Module._nodeModulePaths(path.dirname(filename)); // eslint-disable-line no-underscore-dangle
        mod._compile("module.exports = require;", filename); // eslint-disable-line no-underscore-dangle
        return mod.exports;
    })
);

module.exports = {

    /**
     * Resolves a Node module relative to another module
     * @param {string} moduleName The name of a Node module, or a path to a Node module.
     *
     * @param {string} relativeToPath An absolute path indicating the module that `moduleName` should be resolved relative to. This must be
     * a file rather than a directory, but the file need not actually exist.
     * @returns {string} The absolute path that would result from calling `require.resolve(moduleName)` in a file located at `relativeToPath`
     */
    resolve(moduleName, relativeToPath) {
        try {
            return createRequire(relativeToPath).resolve(moduleName);
        } catch (error) {
            if (
                typeof error === "object" &&
                error !== null &&
                error.code === "MODULE_NOT_FOUND" &&
                !error.requireStack &&
                error.message.includes(moduleName)
            ) {
                error.message += `\nRequire stack:\n- ${relativeToPath}`;
            }
            throw error;
        }
    }
};
