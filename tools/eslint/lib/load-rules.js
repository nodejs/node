/**
 * @fileoverview Module for loading rules from files and directories.
 * @author Michael Ficarra
 */

"use strict";

//------------------------------------------------------------------------------
// Requirements
//------------------------------------------------------------------------------

var fs = require("fs"),
    path = require("path");

//------------------------------------------------------------------------------
// Public Interface
//------------------------------------------------------------------------------

/**
 * Load all rule modules from specified directory.
 * @param {String} [rulesDir] Path to rules directory, may be relative. Defaults to `lib/rules`.
 * @returns {Object} Loaded rule modules by rule ids (file names).
 */
module.exports = function(rulesDir) {
    if (!rulesDir) {
        rulesDir = path.join(__dirname, "rules");
    } else {
        rulesDir = path.resolve(process.cwd(), rulesDir);
    }

    var rules = Object.create(null);
    fs.readdirSync(rulesDir).forEach(function(file) {
        if (path.extname(file) !== ".js") {
            return;
        }
        rules[file.slice(0, -3)] = require(path.join(rulesDir, file));
    });
    return rules;
};
