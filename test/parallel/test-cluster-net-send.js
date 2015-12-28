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

  var socket;
  var cbcalls = 0;
  function socketConnected() {
    if (++cbcalls === 2)
      process.send('handle', socket);
  }

  var server = net.createServer(function(c) {
    process.once('message', function(msg) {
      assert.equal(msg, 'got');
      c.end('hello');
    });
    socketConnected();
  });
  server.listen(common.PORT, function() {
    socket = net.connect(common.PORT, '127.0.0.1', socketConnected);
  });

  process.on('disconnect', function() {
    process.exit();
    server.close();
  });
}
