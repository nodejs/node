/**
 * @fileoverview Config to enable all rules.
 * @author Robert Fletcher
 */

"use strict";

//------------------------------------------------------------------------------
// Requirements
//------------------------------------------------------------------------------

let fs = require("fs"),
    path = require("path");

//------------------------------------------------------------------------------
// Helpers
//------------------------------------------------------------------------------

let ruleFiles = fs.readdirSync(path.resolve(__dirname, "../lib/rules"));
let enabledRules = ruleFiles.reduce(function(result, filename) {
    if (path.extname(filename) === ".js") {
        result[path.basename(filename, ".js")] = "error";
    }
    return result;
}, {});

//------------------------------------------------------------------------------
// Public Interface
//------------------------------------------------------------------------------

module.exports = { rules: enabledRules };
