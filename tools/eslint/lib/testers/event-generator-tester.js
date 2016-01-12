/**
 * @fileoverview Helpers to test EventGenerator interface.
 * @author Toru Nagashima
 * @copyright 2015 Toru Nagashima. All rights reserved.
 * See LICENSE file in root directory for full license.
 */
"use strict";

/* global describe, it */

//------------------------------------------------------------------------------
// Requirements
//------------------------------------------------------------------------------

var assert = require("assert");

//------------------------------------------------------------------------------
// Public Interface
//------------------------------------------------------------------------------

module.exports = {
    /**
     * Overrideable `describe` function to test.
     * @param {string} text - A description.
     * @param {function} method - A test logic.
     * @returns {any} The returned value with the test logic.
     */
    describe: (typeof describe === "function") ? describe : function(text, method) {
        return method.apply(this);
    },

    /**
     * Overrideable `it` function to test.
     * @param {string} text - A description.
     * @param {function} method - A test logic.
     * @returns {any} The returned value with the test logic.
     */
    it: (typeof it === "function") ? it : function(text, method) {
        return method.apply(this);
    },

    /**
     * Does some tests to check a given object implements the EventGenerator interface.
     * @param {object} instance - An object to check.
     * @returns {void}
     */
    testEventGeneratorInterface: function(instance) {
        this.describe("should implement EventGenerator interface", function() {
            this.it("should have `emitter` property.", function() {
                assert.equal(typeof instance.emitter, "object");
                assert.equal(typeof instance.emitter.emit, "function");
            });

            this.it("should have `enterNode` property.", function() {
                assert.equal(typeof instance.enterNode, "function");
            });

            this.it("should have `leaveNode` property.", function() {
                assert.equal(typeof instance.leaveNode, "function");
            });
        }.bind(this));
    }
};
