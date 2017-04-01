/**
 * @author Titus Wormer
 * @copyright 2015-2016 Titus Wormer
 * @license MIT
 * @module unified-engine:file-set-pipeline
 * @fileoverview Process a collection of files.
 */

'use strict';

/* Dependencies. */
var trough = require('trough');
var configure = require('./configure');
var fileSystem = require('./file-system');
var stdin = require('./stdin');
var transform = require('./transform');
var log = require('./log');

/* Expose. */
module.exports = trough()
  .use(configure)
  .use(fileSystem)
  .use(stdin)
  .use(transform)
  .use(log);
