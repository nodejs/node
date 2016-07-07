/**
 * @fileoverview Config to enable all rules.
 * @author Robert Fletcher
 */

"use strict";

//------------------------------------------------------------------------------
// Requirements
//------------------------------------------------------------------------------

var fs = require("fs"),
    path = require("path");

//------------------------------------------------------------------------------
// Helpers
//------------------------------------------------------------------------------

var ruleFiles = fs.readdirSync(path.resolve(__dirname, "../lib/rules"));
var enabledRules = ruleFiles.reduce(function(result, filename) {
    result[path.basename(filename, ".js")] = "error";
    return result;
}, {});

//------------------------------------------------------------------------------
// Public Interface
//------------------------------------------------------------------------------

module.exports = { rules: enabledRules };
