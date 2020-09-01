/**
 * @fileoverview Config to enable all rules.
 * @author Robert Fletcher
 */

"use strict";

//------------------------------------------------------------------------------
// Requirements
//------------------------------------------------------------------------------

// FIXME: "../lib/rules" doesn't exist in this package
const builtInRules = require("../lib/rules");

//------------------------------------------------------------------------------
// Helpers
//------------------------------------------------------------------------------

const allRules = {};

for (const [ruleId, rule] of builtInRules) {
    if (!rule.meta.deprecated) {
        allRules[ruleId] = "error";
    }
}

//------------------------------------------------------------------------------
// Public Interface
//------------------------------------------------------------------------------

/** @type {import("../lib/shared/types").ConfigData} */
module.exports = { rules: allRules };
