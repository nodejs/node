'use strict';
// Installing a custom uncaughtException handler should override the default
// one that the cluster module installs.
// https://github.com/joyent/node/issues/2556

const common = require('../common');
var assert = require('assert');
var cluster = require('cluster');
var fork = require('child_process').fork;

var MAGIC_EXIT_CODE = 42;

var isTestRunner = process.argv[2] != 'child';

if (isTestRunner) {
  var master = fork(__filename, ['child']);
  master.on('exit', common.mustCall(function(code) {
    assert.strictEqual(code, MAGIC_EXIT_CODE);
  }));
} else if (cluster.isMaster) {
  process.on('uncaughtException', function() {
    process.nextTick(function() {
      process.exit(MAGIC_EXIT_CODE);
    });
  });

  cluster.fork();
  throw new Error('kill master');
} else { // worker
  process.exit();
}
