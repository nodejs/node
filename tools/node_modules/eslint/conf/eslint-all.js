/**
 * @fileoverview Config to enable all rules.
 * @author Robert Fletcher
 */

"use strict";

//------------------------------------------------------------------------------
// Requirements
//------------------------------------------------------------------------------

const load = require("../lib/load-rules"),
    Rules = require("../lib/rules");
const rules = new Rules();

//------------------------------------------------------------------------------
// Helpers
//------------------------------------------------------------------------------

const enabledRules = Object.keys(load()).reduce((result, ruleId) => {
    if (!rules.get(ruleId).meta.deprecated) {
        result[ruleId] = "error";
    }
    return result;
}, {});

//------------------------------------------------------------------------------
// Public Interface
//------------------------------------------------------------------------------

module.exports = { rules: enabledRules };
