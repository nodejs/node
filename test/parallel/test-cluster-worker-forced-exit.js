'use strict';
const common = require('../common');
var assert = require('assert');
var cluster = require('cluster');

var SENTINEL = 42;

// workers forcibly exit when control channel is disconnected, if
// their .exitedAfterDisconnect flag isn't set
//
// test this by:
//
// 1 setup worker to wait a short time after disconnect, and exit
//   with a sentinel value
// 2 disconnect worker with cluster's disconnect, confirm sentinel
// 3 disconnect worker with child_process's disconnect, confirm
//   no sentinel value
if (cluster.isWorker) {
  process.on('disconnect', function(msg) {
    setTimeout(function() {
      process.exit(SENTINEL);
    }, 10);
  });
  return;
}

checkUnforced();
checkForced();

function checkUnforced() {
  cluster.fork()
    .on('online', function() { this.disconnect(); })
    .on('exit', common.mustCall(function(status) {
      assert.strictEqual(status, SENTINEL);
    }));
}

function checkForced() {
  cluster.fork()
    .on('online', function() { this.process.disconnect(); })
    .on('exit', common.mustCall(function(status) {
      assert.strictEqual(status, 0);
    }));
}
