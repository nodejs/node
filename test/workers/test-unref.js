// Flags: --experimental-workers
'use strict';

var common = require('../common');
common.use();
var assert = require('assert');
var Worker = require('worker');
var checks = 0;

if (process.isMainThread) {
  var aWorker = new Worker(__filename);
  aWorker.on('exit', function() {
    checks++;
  });
  aWorker.on('message', function() {
    checks++;
    setTimeout(function() {
      checks++;
      aWorker.terminate();
    }, 5);
  });
  process.on('beforeExit', function() {
    assert.equal(0, checks);
  });
  aWorker.unref();
} else {
  setInterval(function() {
    Worker.postMessage();
  }, 5);
}
