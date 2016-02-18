// Flags: --experimental-workers
'use strict';

var common = require('../common');
common.use();
var assert = require('assert');
var Worker = require('worker');
var ids = [];

if (process.isMainThread) {
  var aWorker = new Worker(__filename);
  aWorker.postMessage({
    init: true,
    subWorker: false
  });
  aWorker.on('message', function(data) {
    ids.push(data.id);
    if (ids.length === 4) {
      // Terminating the main worker should terminate its 4 sub-workers
      aWorker.terminate();
    }
  });
  process.on('beforeExit', function() {
    assert.deepEqual([0, 1, 2, 3].sort(), ids.sort());
  });
} else {
  Worker.on('message', function(data) {
    if (data.init) {
      if (data.subWorker) {
        subWorker(data.id);
      } else {
        mainWorker();
      }
    }
  });

  var mainWorker = function() {
    var l = 4;
    while (l--) {
      var worker = new Worker(__filename);
      worker.postMessage({
        init: true,
        subWorker: true,
        id: l
      });
      worker.on('message', function(payload) {
        Worker.postMessage(payload);
      });
    }
  };

  var subWorker = function(id) {
    Worker.postMessage({id: id});
    while (true);
  };
}
