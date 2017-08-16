/**
 * @fileoverview Expose out ESLint and CLI to require.
 * @author Ian Christian Myers
 */

"use strict";

const Linter = require("./linter");

module.exports = {
    linter: new Linter(),
    Linter,
    CLIEngine: require("./cli-engine"),
    RuleTester: require("./testers/rule-tester"),
    SourceCode: require("./util/source-code")
};
