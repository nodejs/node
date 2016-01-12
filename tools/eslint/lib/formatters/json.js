/**
 * @fileoverview JSON reporter
 * @author Burak Yigit Kaya aka BYK
 * @copyright 2015 Burak Yigit Kaya. All rights reserved.
 */
"use strict";

//------------------------------------------------------------------------------
// Public Interface
//------------------------------------------------------------------------------

module.exports = function(results) {
    return JSON.stringify(results);
};
