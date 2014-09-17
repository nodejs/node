var cluster = require('cluster');
var assert = require('assert');
var net = require('net');

if (cluster.isMaster) {
  var worker = cluster.fork();
  assert.ok(!worker.isDead(),
            "isDead() should return false right after the worker has been " +
            "created.");

  worker.on('exit', function() {
      assert.ok(!worker.isConnected(),
                "After an event has been emitted, " +
                "isDead should return true");
  })

  worker.on('message', function(msg) {
      if (msg === 'readyToDie') {
          worker.kill();
      }
  });

} else if (cluster.isWorker) {
  assert.ok(!cluster.worker.isDead(),
            "isDead() should return false when called from within a worker");
  process.send('readyToDie');
}
