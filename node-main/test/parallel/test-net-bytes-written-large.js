'use strict';
const common = require('../common');
const assert = require('assert');
const net = require('net');

// Regression test for https://github.com/nodejs/node/issues/19562:
// Writing to a socket first tries to push through as much data as possible
// without blocking synchronously, and, if that is not enough, queues more
// data up for asynchronous writing.
// Check that `bytesWritten` accounts for both parts of a write.

const N = 10000000;
{
  // Variant 1: Write a Buffer.
  const server = net.createServer(common.mustCall((socket) => {
    socket.end(Buffer.alloc(N), common.mustCall(() => {
      assert.strictEqual(socket.bytesWritten, N);
    }));
    assert.strictEqual(socket.bytesWritten, N);
  })).listen(0, common.mustCall(() => {
    const client = net.connect(server.address().port);
    client.resume();
    client.on('close', common.mustCall(() => {
      assert.strictEqual(client.bytesRead, N);
      server.close();
    }));
  }));
}

{
  // Variant 2: Write a string.
  const server = net.createServer(common.mustCall((socket) => {
    socket.end('a'.repeat(N), common.mustCall(() => {
      assert.strictEqual(socket.bytesWritten, N);
    }));
    assert.strictEqual(socket.bytesWritten, N);
  })).listen(0, common.mustCall(() => {
    const client = net.connect(server.address().port);
    client.resume();
    client.on('close', common.mustCall(() => {
      assert.strictEqual(client.bytesRead, N);
      server.close();
    }));
  }));
}

{
  // Variant 2: writev() with mixed data.
  const server = net.createServer(common.mustCall((socket) => {
    socket.cork();
    socket.write('a'.repeat(N));
    assert.strictEqual(socket.bytesWritten, N);
    socket.write(Buffer.alloc(N));
    assert.strictEqual(socket.bytesWritten, 2 * N);
    socket.end('', common.mustCall(() => {
      assert.strictEqual(socket.bytesWritten, 2 * N);
    }));
    socket.uncork();
  })).listen(0, common.mustCall(() => {
    const client = net.connect(server.address().port);
    client.resume();
    client.on('close', common.mustCall(() => {
      assert.strictEqual(client.bytesRead, 2 * N);
      server.close();
    }));
  }));
}
