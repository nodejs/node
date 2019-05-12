/**
 * @fileoverview Expose out ESLint and CLI to require.
 * @author Ian Christian Myers
 */

"use strict";

const { CLIEngine } = require("./cli-engine");
const { Linter } = require("./linter");
const { RuleTester } = require("./rule-tester");
const { SourceCode } = require("./source-code");

module.exports = {
    Linter,
    CLIEngine,
    RuleTester,
    SourceCode
};

// DOTO: remove deprecated API.
let deprecatedLinterInstance = null;

Object.defineProperty(module.exports, "linter", {
    enumerable: false,
    get() {
        if (!deprecatedLinterInstance) {
            deprecatedLinterInstance = new Linter();
        }

        return deprecatedLinterInstance;
    }
});
