// Flags: --experimental-workers
'use strict';

var common = require('../common');
common.use();
var assert = require('assert');
var Worker = require('worker');
var checks = 0;

if (process.isMainThread) {
  var timer = setInterval(function() {}, 1000);
  var aWorker = new Worker(__filename);
  aWorker.on('exit', function() {
    checks++;
  });
  aWorker.on('message', function() {
    checks++;
    setTimeout(function() {
      checks++;
      aWorker.terminate(function() {
        checks++;
        clearInterval(timer);
      });
    }, 5);
  });
  process.on('beforeExit', function() {
    assert.equal(4, checks);
  });
  aWorker.unref();
  aWorker.postMessage();
} else {
  Worker.on('message', function() {
    setTimeout(function() {
      Worker.postMessage();
    }, 1);
  });
}
