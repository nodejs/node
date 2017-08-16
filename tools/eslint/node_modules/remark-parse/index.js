/**
 * @author Titus Wormer
 * @copyright 2015-2016 Titus Wormer
 * @license MIT
 * @module remark:parse
 * @fileoverview Markdown parser.
 */

'use strict';

/* eslint-env commonjs */

/* Dependencies. */
var unherit = require('unherit');
var Parser = require('./lib/parser.js');

/**
 * Attacher.
 *
 * @param {unified} processor - Unified processor.
 */
function parse(processor) {
    processor.Parser = unherit(Parser);
}

/* Patch `Parser`. */
parse.Parser = Parser;

/* Expose */
module.exports = parse;
