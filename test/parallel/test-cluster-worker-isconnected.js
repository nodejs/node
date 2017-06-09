'use strict';
require('../common');
const cluster = require('cluster');
const assert = require('assert');

if (cluster.isMaster) {
  const worker = cluster.fork();

  assert.ok(worker.isConnected(),
            'isConnected() should return true as soon as the worker has ' +
            'been created.');

  worker.on('disconnect', function() {
    assert.ok(!worker.isConnected(),
              'After a disconnect event has been emitted, ' +
              'isConncted should return false');
  });

  worker.on('message', function(msg) {
    if (msg === 'readyToDisconnect') {
      worker.disconnect();
    }
  });

} else {
  assert.ok(cluster.worker.isConnected(),
            'isConnected() should return true from within a worker at all ' +
            'times.');

  cluster.worker.process.on('disconnect', function() {
    assert.ok(!cluster.worker.isConnected(),
              'isConnected() should return false from within a worker ' +
              'after its underlying process has been disconnected from ' +
              'the master');
  });

  process.send('readyToDisconnect');
}
