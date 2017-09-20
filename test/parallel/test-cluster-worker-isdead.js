'use strict';
require('../common');
const cluster = require('cluster');
const assert = require('assert');

if (cluster.isMaster) {
  const worker = cluster.fork();
  assert.ok(
    !worker.isDead(),
    'isDead() should return false right after the worker has been created.');

  worker.on('exit', function() {
    assert.ok(worker.isDead(),
              'After an event has been emitted, isDead should return true');
  });

  worker.on('message', function(msg) {
    if (msg === 'readyToDie') {
      worker.kill();
    }
  });

} else if (cluster.isWorker) {
  assert.ok(!cluster.worker.isDead(),
            'isDead() should return false when called from within a worker');
  process.send('readyToDie');
}
