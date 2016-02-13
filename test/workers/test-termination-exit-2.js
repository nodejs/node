// Flags: --experimental-workers
'use strict';

var common = require('../common');
common.use();
var assert = require('assert');
var Worker = require('worker');
var checks = 0;

if (process.isMainThread) {
  var aWorker = new Worker(__filename, {keepAlive: false});
  aWorker.on('exit', function(code) {
    assert.equal(1337, code);
    checks++;
  });
  aWorker.on('message', function(data) {
    assert.equal(data, 0);
    checks++;
  });
  process.on('beforeExit', function() {
    assert.equal(2, checks);
  });
} else {
  var emits = 0;
  process.on('beforeExit', function() {
    setInterval(function() {
      Worker.postMessage({hello: 'world'});
    }, 5000);
    setImmediate(function f() {
      Worker.postMessage({hello: 'world'});
      setImmediate(f);
    });
    process.exit(1337);
  });
  process.on('exit', function() {
    Worker.postMessage(emits++);
  });
}
