'use strict';
// test-cluster-worker-init.js
// verifies that, when a child process is forked, the cluster.worker
// object can receive messages as expected

require('../common');
var assert = require('assert');
var cluster = require('cluster');
var msg = 'foo';

if (cluster.isMaster) {
  var worker = cluster.fork();
  var timer = setTimeout(function() {
    assert(false, 'message not received');
  }, 5000);

  timer.unref();

  worker.on('message', function(message) {
    assert(message, 'did not receive expected message');
    worker.disconnect();
  });

  worker.on('online', function() {
    worker.send(msg);
  });
} else {
  // GH #7998
  cluster.worker.on('message', function(message) {
    process.send(message === msg);
  });
}
