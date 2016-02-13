// Flags: --experimental-workers
'use strict';

// Uncaught exceptions inside worker bubble to the parent thread's worker
// object's 'error' event.

var common = require('../common');
common.use();
var assert = require('assert');
var Worker = require('worker');
var checks = 0;

if (process.isMainThread) {
  var aWorker = new Worker(__filename, {keepAlive: false});

  aWorker.on('error', function(e) {
    assert(e instanceof ReferenceError);
    assert.equal('ReferenceError: undefinedReference is not defined',
                 e.toString());
    checks++;
  });

  aWorker.on('message', function(data) {
    assert.deepEqual({hello: 'world'}, data);
    checks++;
  });

  process.on('beforeExit', function() {
    assert.equal(2, checks);
  });
} else {
  setTimeout(function() {
    Worker.postMessage({hello: 'world'});
  }, 5);
  undefinedReference;
}
