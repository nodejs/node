/**
 * @author Titus Wormer
 * @copyright 2015-2016 Titus Wormer
 * @license MIT
 * @module remark
 * @version 3.2.2
 * @fileoverview Markdown processor powered by plugins.
 */

'use strict';

/* eslint-env commonjs */

/*
 * Dependencies.
 */

var unified = require('unified');
var Parser = require('./lib/parse.js');
var Compiler = require('./lib/stringify.js');
var escape = require('./lib/escape.json');

/*
 * Exports.
 */

module.exports = unified({
    'name': 'mdast',
    'Parser': Parser,
    'Compiler': Compiler,
    'data': {
        'escape': escape
    }
});
