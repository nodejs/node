'use strict';
const common = require('../common');
const assert = require('assert');
const http = require('http');
const { Duplex } = require('stream');

// Refs: https://github.com/nodejs/node/issues/66781
// A chunked response written with multi-byte string chunks must emit 'drain'
// after write() returned false. write() decides backpressure from the byte
// length of the auto-corked chunks, while net.Socket counts string chunks in
// UTF-16 code units, so the socket may stay below its own highWaterMark and
// never emit 'drain' itself once the coalesced chunks are flushed.

// Behaves like net.Socket for this purpose: strings are counted in UTF-16
// code units and writes complete asynchronously, so flushed data is still in
// flight when uncork() returns.
function createSocket() {
  const pending = [];
  const socket = new Duplex({
    decodeStrings: false,
    read() {},
    write(chunk, encoding, callback) {
      pending.push(callback);
    },
  });
  // Complete one write per turn of the event loop.
  (function flush() {
    pending.shift()?.();
    if (!socket.destroyed) setImmediate(flush);
  })();
  return socket;
}

const row = 'a'.repeat(100) + 'ع'.repeat(150) + '\n'; // 401 bytes

{
  const ROWS = 2000;
  const socket = createSocket();
  const server = http.createServer(common.mustCall(async (req, res) => {
    let drains = 0;
    for (let i = 0; i < ROWS; i++) {
      if (!res.write(row)) {
        assert.strictEqual(res.writableNeedDrain, true);
        await new Promise((resolve) => res.once('drain', resolve));
        assert.strictEqual(res.writableLength, 0);
        assert.strictEqual(res.writableNeedDrain, false);
        drains++;
      }
    }
    assert.ok(drains > 0);
    res.end(common.mustCall(() => socket.destroy()));
  }));

  server.emit('connection', socket);
  socket.push('GET / HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n');
}

// 'drain' must not be emitted once the response has been destroyed from a
// write callback.
{
  const socket = createSocket();
  const server = http.createServer(common.mustCall((req, res) => {
    res.on('drain', common.mustNotCall());
    const ret = res.write(row.repeat(200), common.mustCall(() => {
      res.destroy();
    }));
    assert.strictEqual(ret, false);
  }));

  server.emit('connection', socket);
  socket.push('GET / HTTP/1.1\r\nHost: localhost\r\n\r\n');
}
