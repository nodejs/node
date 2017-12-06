'use strict';

var trough = require('trough');
var configure = require('./configure');
var fileSystem = require('./file-system');
var stdin = require('./stdin');
var transform = require('./transform');
var log = require('./log');

module.exports = trough()
  .use(configure)
  .use(fileSystem)
  .use(stdin)
  .use(transform)
  .use(log);
