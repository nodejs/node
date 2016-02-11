/**
 * @author Titus Wormer
 * @copyright 2015-2016 Titus Wormer
 * @license MIT
 * @module remark:cli:spinner
 * @version 3.2.2
 * @fileoverview A spinner for stdout(4).
 */

'use strict';

/* eslint-env node */

/*
 * Dependencies.
 */

var logUpdate = require('log-update');
var spinner = require('elegant-spinner');

/**
 * Spinner.
 *
 * @example
 *   var spinner = new Spinner();
 *
 * @constructor
 * @class {FileSet}
 */
function Spinner() {}

/**
 * Start spinning.
 *
 * @example
 *   new Spinner().start();
 *
 * @this {Spinner}
 * @return {Spinner} - Self.
 */
function start() {
    var self = this;
    var spin = spinner();

    if (!self.id) {
        self.id = setInterval(function () {
            logUpdate(spin());
        }, 100);
    }

    return self;
}

/**
 * Stop spinning.
 *
 * @example
 *   spinner = new Spinner().start();
 *   setTimeout(function () { spinner.stop(); }, 1000);
 *
 * @this {Spinner}
 * @return {Spinner} - Self.
 */
function stop() {
    var self = this;

    if (self.id) {
        logUpdate('');
        logUpdate.clear();
        clearInterval(self.id);
        self.id = null;
    }

    return self;
}

/*
 * Attach.
 */

var proto = Spinner.prototype;

proto.start = start;
proto.stop = stop;

/*
 * Expose.
 */

module.exports = Spinner;
