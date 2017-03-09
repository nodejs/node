'use strict';
const common = require('../common');
const assert = require('assert');
const cluster = require('cluster');

if (!cluster.isMaster) {
  process.exit(42);
} else {
  const worker = cluster.fork();
  worker.on('exit', common.mustCall(function(exitCode, signalCode) {
    assert.equal(exitCode, 42);
    assert.equal(signalCode, null);
  }));
  cluster.on('exit', common.mustCall(function(worker_) {
    assert.equal(worker_, worker);
  }));
}
