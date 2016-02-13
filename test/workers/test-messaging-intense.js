// Flags: --experimental-workers
'use strict';

var common = require('../common');
common.use();
var assert = require('assert');
var Worker = require('worker');

// Test that both primary and backup queue work
if (process.isMainThread) {
  const TO_SEND = 4096;
  const POST_MORE_THRESHOLD = 1024;
  // 2 is the number of messages posted back by the worker for each message
  // it receives
  const EXPECTED_TO_RECEIVE = ((TO_SEND / POST_MORE_THRESHOLD * 2) - 1) *
                              POST_MORE_THRESHOLD * 2 + TO_SEND * 2;

  var messagesReceived = 0;
  var worker = new Worker(__filename);
  worker.on('message', function() {
    messagesReceived++;
    if ((messagesReceived % POST_MORE_THRESHOLD) === 0 &&
         messagesReceived < (TO_SEND * 2)) {
      var l = POST_MORE_THRESHOLD;
      while (l--)
        worker.postMessage();
    }

    if (messagesReceived == EXPECTED_TO_RECEIVE)
      worker.terminate();
  });

  var l = TO_SEND;
  while (l--)
    worker.postMessage();

  process.on('beforeExit', function() {
    assert.equal(EXPECTED_TO_RECEIVE, messagesReceived);
  });
} else {
  Worker.on('message', function() {
    Worker.postMessage();
    process.nextTick(function() {
      Worker.postMessage();
    });
  });
}
