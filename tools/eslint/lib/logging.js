/**
 * @fileoverview Handle logging for Eslint
 * @author Gyandeep Singh
 * @copyright 2015 Gyandeep Singh. All rights reserved.
 */
"use strict";

/* istanbul ignore next */
module.exports = {
    /**
     * Cover for console.log
     * @returns {void}
     */
    info: function() {
        console.log.apply(console, Array.prototype.slice.call(arguments));
    },

    /**
     * Cover for console.error
     * @returns {void}
     */
    error: function() {
        console.error.apply(console, Array.prototype.slice.call(arguments));
    }
};
