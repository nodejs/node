/**
 * @author Titus Wormer
 * @copyright 2015-2016 Titus Wormer
 * @license MIT
 * @module remark:cli:file-set-pipeline
 * @version 3.2.2
 * @fileoverview Process a collection of files.
 */

'use strict';

/* eslint-env node */

/*
 * Dependencies.
 */

var ware = require('ware');
var configure = require('./configure');
var fileSystem = require('./file-system');
var stdin = require('./stdin');
var transform = require('./transform');
var log = require('./log');

/*
 * Middleware.
 */

var fileSetPipeline = ware()
    .use(configure)
    .use(fileSystem)
    .use(stdin)
    .use(transform)
    .use(log);

/*
 * Expose.
 */

module.exports = fileSetPipeline;
