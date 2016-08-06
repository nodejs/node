/**
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @module remark:stringify
 * @fileoverview Markdown Compiler.
 */

'use strict';

/* Dependencies. */
var unherit = require('unherit');
var Compiler = require('./lib/compiler.js');

/* Expose. */
module.exports = stringify;

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
