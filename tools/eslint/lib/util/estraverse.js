/**
 * @fileoverview Patch for estraverse
 * @author Toru Nagashima
 * @copyright 2015 Toru Nagashima. All rights reserved.
 * See LICENSE file in root directory for full license.
 */
"use strict";

//------------------------------------------------------------------------------
// Requirements
//------------------------------------------------------------------------------

var estraverse = require("estraverse"),
    jsxKeys = require("estraverse-fb/keys");

//------------------------------------------------------------------------------
// Helers
//------------------------------------------------------------------------------

var experimentalKeys = {
    ExperimentalRestProperty: ["argument"],
    ExperimentalSpreadProperty: ["argument"]
};

/**
 * Adds a given keys to Syntax and VisitorKeys of estraverse.
 *
 * @param {object} keys - Key definitions to add.
 *   This is an object as map.
 *   Keys are the node type.
 *   Values are an array of property names to visit.
 * @returns {void}
 */
function installKeys(keys) {
    for (var key in keys) {
        if (keys.hasOwnProperty(key)) {
            estraverse.Syntax[key] = key;
            if (keys[key]) {
                estraverse.VisitorKeys[key] = keys[key];
            }
        }
    }
}

// Add JSX node types.
installKeys(jsxKeys);
// Add Experimental node types.
installKeys(experimentalKeys);

//------------------------------------------------------------------------------
// Public Interface
//------------------------------------------------------------------------------

module.exports = estraverse;
