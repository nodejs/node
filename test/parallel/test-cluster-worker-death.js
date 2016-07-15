'use strict';
const common = require('../common');
var assert = require('assert');
var cluster = require('cluster');

if (!cluster.isMaster) {
  process.exit(42);
} else {
  var worker = cluster.fork();
  worker.on('exit', common.mustCall(function(exitCode, signalCode) {
    assert.equal(exitCode, 42);
    assert.equal(signalCode, null);
  }));
  cluster.on('exit', common.mustCall(function(worker_) {
    assert.equal(worker_, worker);
  }));
}
