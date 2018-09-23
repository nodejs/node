'use strict';
var common = require('../common');
var assert = require('assert');
var fork = require('child_process').fork;
var net = require('net');

if (process.argv[2] !== 'child') {
  console.error('[%d] master', process.pid);

  var worker = fork(__filename, ['child']);
  var called = false;

  worker.once('message', function(msg, handle) {
    assert.equal(msg, 'handle');
    assert.ok(handle);
    worker.send('got');

    handle.on('data', function(data) {
      called = true;
      assert.equal(data.toString(), 'hello');
    });

    handle.on('end', function() {
      worker.kill();
    });
  });

  process.once('exit', function() {
    assert.ok(called);
  });
} else {
  console.error('[%d] worker', process.pid);

  var server = net.createServer(function(c) {
    process.once('message', function(msg) {
      assert.equal(msg, 'got');
      c.end('hello');
    });
  });
  server.listen(common.PORT, function() {
    var socket = net.connect(common.PORT, '127.0.0.1', function() {
      process.send('handle', socket);
    });
  });

  process.on('disconnect', function() {
    process.exit();
    server.close();
  });
}
