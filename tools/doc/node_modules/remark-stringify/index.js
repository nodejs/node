/**
 * @author Titus Wormer
 * @copyright 2015-2016 Titus Wormer
 * @license MIT
 * @module remark:stringify
 * @fileoverview Markdown Compiler.
 */

'use strict';

/* eslint-env commonjs */

/* Dependencies. */
var unherit = require('unherit');
var Compiler = require('./lib/compiler.js');

/**
 * Attacher.
 *
 * @param {unified} processor - Unified processor.
 */
function stringify(processor) {
    processor.Compiler = unherit(Compiler);
}

/* Patch `Compiler`. */
stringify.Compiler = Compiler;

/* Expose. */
module.exports = stringify;
