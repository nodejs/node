/**
 * @fileoverview Expose out ESLint and CLI to require.
 * @author Ian Christian Myers
 */

"use strict";

module.exports = {
    linter: require("./eslint"),
    cli: require("./cli"),
    CLIEngine: require("./cli-engine"),
    validator: require("./config-validator")
};
