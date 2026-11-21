'use strict';

const common = require('../common');
const net = require('net');
const assert = require('assert');

// Regression test.
//
// When an `onread` callback transfers a view of the reusable buffer into a byte
// `ReadableStream`, the transfer detaches the buffer's backing store. The socket
// must not keep writing subsequent peer data into the detached backing store,
// otherwise it corrupts the chunk the application has retained.

let controller;
const stream = new ReadableStream({
  type: 'bytes',
  start(c) {
    controller = c;
  },
});
const reader = stream.getReader();

const server = net.createServer(common.mustCall((socket) => {
  socket.setNoDelay(true);
  socket.write('first');
  // Send more data shortly after the client transfers the first chunk.
  const timer = setTimeout(() => {
    socket.write('Z'.repeat(1024));
  }, 50);
  socket.on('close', () => clearTimeout(timer));
  socket.on('error', common.mustNotCall());
}));

server.listen(0, common.mustCall(() => {
  const port = server.address().port;

  let transferred = false;

  const client = net.createConnection({
    host: '127.0.0.1',
    port,
    onread: {
      buffer: Buffer.alloc(4096),
      callback(nread, buf) {
        // Enqueueing a view transfers (and thereby detaches) the underlying
        // ArrayBuffer. An affected binary then keeps writing later peer data
        // over this chunk's memory.
        if (transferred) return;
        transferred = true;
        const view = new Uint8Array(buf.buffer, buf.byteOffset, nread);
        controller.enqueue(view);
      },
    },
  });

  client.on('error', common.mustNotCall());

  (async () => {
    const { value: chunk, done } = await reader.read();
    assert.strictEqual(done, false);

    // The first peer write ("first", 5 bytes) is delivered intact.
    assert.strictEqual(chunk[0], 0x66); // 'f'
    assert.strictEqual(Buffer.from(chunk).toString(), 'first');

    // Give the second peer write time to reach the client. Reading has stopped,
    // so the retained chunk must not be overwritten.
    await new Promise((resolve) => setTimeout(resolve, 200));

    assert.strictEqual(chunk[0], 0x66,
                       `retained chunk was corrupted: 0x${chunk[0].toString(16)}`);
    assert.strictEqual(Buffer.from(chunk).toString(), 'first');

    client.end();
    server.close();
  })().then(common.mustCall());
}));
