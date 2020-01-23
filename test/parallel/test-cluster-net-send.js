// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.

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
    server.close();
  });
}
