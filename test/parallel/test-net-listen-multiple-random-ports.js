'use strict';
require('../common');
var assert = require('assert');
var cluster = require('cluster');
var net = require('net');

function noop() {}

if (cluster.isMaster) {
  var worker1 = cluster.fork();
  var worker2 = cluster.fork();

  worker1.on('message', onMessage);
  worker2.on('message', onMessage);
  function onMessage(obj) {
    assert.strictEqual(typeof obj, 'object');
    assert.strictEqual(obj.msg, 'success');
    assert.strictEqual(typeof obj.port, 'number');
    assert.ok(obj.port !== 0, 'Expected non-zero port number from worker');
    this.listens = (this.listens || 0) + 1;
    if (worker1.listens === 2 && worker2.listens === 2) {
      worker1.kill();
      worker2.kill();
    }
  }
} else {
  net.createServer(noop).on('error', function(err) {
    // no errors expected
    process.send('server1:' + err.code);
  }).listen({
    host: 'localhost',
    port: 0,
    exclusive: false
  }, function() {
    process.send({
      msg: 'success',
      port: this.address().port,
    });
  })

  net.createServer(noop).on('error', function(err) {
    // no errors expected
    process.send('server2:' + err.code);
  }).listen({
    host: 'localhost',
    port: 0,
    exclusive: false
  }, function() {
    process.send({
      msg: 'success',
      port: this.address().port,
    });
  });
}
