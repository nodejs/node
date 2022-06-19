/**
 * @fileoverview APIs that are not officially supported by ESLint.
 *      These APIs may change or be removed at any time. Use at your
 *      own risk.
 * @author Nicholas C. Zakas
 */

"use strict";

//-----------------------------------------------------------------------------
// Requirements
//-----------------------------------------------------------------------------

const { FileEnumerator } = require("./cli-engine/file-enumerator");

//-----------------------------------------------------------------------------
// Exports
//-----------------------------------------------------------------------------

module.exports = {
    builtinRules: require("./rules"),
    FileEnumerator
};
