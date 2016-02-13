// Flags: --experimental-workers
'use strict';

var common = require('../common');
common.use();
var assert = require('assert');
var Worker = require('worker');
var checks = 0;

if (process.isMainThread) {
  var aWorker = new Worker(__filename);
  var gotMessage = false;
  aWorker.on('message', function() {
    if (gotMessage) return;
    gotMessage = true;
    checks++;
    aWorker.postMessage();
    aWorker.postMessage();
    aWorker.postMessage();
    aWorker.postMessage();
    aWorker.terminate(function() {
      checks++;
    });
  });
  process.on('beforeExit', function() {
    assert.equal(2, checks);
  });
} else {
  Worker.on('message', function() {
    while (true)
      Worker.postMessage({hello: 'world'});
  });

  Worker.postMessage();
}
