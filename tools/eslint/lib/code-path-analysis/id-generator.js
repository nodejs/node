/**
 * @fileoverview A class of identifiers generator for code path segments.
 *
 * Each rule uses the identifier of code path segments to store additional
 * information of the code path.
 *
 * @author Toru Nagashima
 * @copyright 2015 Toru Nagashima. All rights reserved.
 * See LICENSE file in root directory for full license.
 */

"use strict";

//------------------------------------------------------------------------------
// Public Interface
//------------------------------------------------------------------------------

/**
 * A generator for unique ids.
 *
 * @constructor
 * @param {string} prefix - Optional. A prefix of generated ids.
 */
function IdGenerator(prefix) {
    this.prefix = String(prefix);
    this.n = 0;
}

/**
 * Generates id.
 *
 * @returns {string} A generated id.
 */
IdGenerator.prototype.next = function() {
    this.n = 1 + this.n | 0;

    /* istanbul ignore if */
    if (this.n < 0) {
        this.n = 1;
    }

    return this.prefix + this.n;
};

module.exports = IdGenerator;
