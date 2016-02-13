// Flags: --experimental-workers
'use strict';

var common = require('../common');
common.use();
var assert = require('assert');
var Worker = require('worker');
var checks = 0;

if (process.isMainThread) {
  var aWorker = new Worker(__filename);
  aWorker.terminate(function() {
    checks++;
  });
  aWorker.on('message', function() {
    assert.fail();
  });
  process.on('beforeExit', function() {
    assert.equal(1, checks);
  });
} else {
  setInterval(function() {
    Worker.postMessage({hello: 'world'});
  }, 5000);
  setImmediate(function f() {
    Worker.postMessage({hello: 'world'});
    setImmediate(f);
  });
}
