/**
 * @fileoverview Shared flags for ESLint.
 */

"use strict";

/**
 * The set of flags that change ESLint behavior with a description.
 * @type {Map<string, string>}
 */
const activeFlags = new Map([
    ["test_only", "Used only for testing."],
    ["unstable_ts_config", "Enable TypeScript configuration files."]
]);

/**
 * The set of flags that used to be active but no longer have an effect.
 * @type {Map<string, string>}
 */
const inactiveFlags = new Map([
    ["test_only_old", "Used only for testing."]
]);

module.exports = {
    activeFlags,
    inactiveFlags
};
