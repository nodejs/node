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
  console.error('[%d] master', process.pid);

  const worker = fork(__filename, ['child']);
  let result = '';

  worker.once('message', common.mustCall((msg, handle) => {
    assert.strictEqual(msg, 'handle');
    assert.ok(handle);
    worker.send('got');

    handle.on('data', (data) => {
      result += data.toString();
    });

    handle.on('end', () => {
      assert.strictEqual(result, 'hello');
      worker.kill();
    });
  }));
} else {
  console.error('[%d] worker', process.pid);

  let socket;
  let cbcalls = 0;
  function socketConnected() {
    if (++cbcalls === 2)
      process.send('handle', socket);
  }

  const server = net.createServer((c) => {
    process.once('message', common.mustCall((msg) => {
      assert.strictEqual(msg, 'got');
      c.end('hello');
    }));
    socketConnected();
  });

  server.listen(common.PORT, () => {
    socket = net.connect(server.address().port, server.address().address,
                         socketConnected);
  });

  process.on('disconnect', () => {
    server.close();
  });
}
