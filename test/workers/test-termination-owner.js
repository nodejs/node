// Flags: --experimental-workers
'use strict';

var common = require('../common');
common.use();
var Worker = require('worker');

// Test that termination of a worker that is in the middle of processing
// messages from its sub-worker works.

if (process.isMainThread) {
  var worker = new Worker(__filename);
  worker.postMessage({main: true});
  worker.on('message', function() {
    worker.terminate();
  });
} else {
  Worker.on('message', function(data) {
    if (data.main) {
      var messagesReceived = 0;
      var subworker = new Worker(__filename);
      subworker.postMessage({main: false});
      subworker.on('message', function() {
        messagesReceived++;

        if (messagesReceived === 512)
          Worker.postMessage();
      });
    } else {
      while (true) Worker.postMessage();
    }
  });
}
