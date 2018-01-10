'use strict';
const common = require('../common');
const assert = require('assert');
const fork = require('child_process').fork;
const net = require('net');

if (process.argv[2] !== 'child') {
  console.error(`[${process.pid}] master`);

  const worker = fork(__filename, ['child']);
  let called = false;

  worker.once('message', common.mustCall(function(msg, handle) {
    assert.strictEqual(msg, 'handle');
    assert.ok(handle);
    worker.send('got');

    handle.on('data', function(data) {
      called = true;
      assert.strictEqual(data.toString(), 'hello');
    });

    handle.on('end', function() {
      worker.kill();
    });
  }));

  process.once('exit', function() {
    assert.ok(called);
  });
} else {
  console.error(`[${process.pid}] worker`);

  let socket;
  let cbcalls = 0;
  function socketConnected() {
    if (++cbcalls === 2)
      process.send('handle', socket);
  }

  const server = net.createServer(function(c) {
    process.once('message', common.mustCall(function(msg) {
      assert.strictEqual(msg, 'got');
      c.end('hello');
    }));
    socketConnected();
  });

  server.listen(0, function() {
    socket = net.connect(server.address().port, '127.0.0.1', socketConnected);
  });

  process.on('disconnect', function() {
    process.exit();
    server.close();
  });
}
