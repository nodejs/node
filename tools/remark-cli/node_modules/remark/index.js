/**
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @module remark
 * @fileoverview Markdown processor powered by plugins.
 */

'use strict';

/* Dependencies. */
var unified = require('unified');
var parse = require('remark-parse');
var stringify = require('remark-stringify');

/* Expose. */
module.exports = unified().use(parse).use(stringify).abstract();
