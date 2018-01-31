// Flags: --expose_internals

'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const assert = require('assert');
const h2 = require('http2');
const net = require('net');

const { kTimeout } = require('internal/timers');

// Tests behavior of the proxied socket on Http2Session

const errMsg = {
  code: 'ERR_HTTP2_NO_SOCKET_MANIPULATION',
  type: Error,
  message: 'HTTP/2 sockets should not be directly manipulated ' +
           '(e.g. read and written)'
};

const server = h2.createServer();

server.on('stream', common.mustCall(function(stream, headers) {
  const socket = stream.session.socket;
  const session = stream.session;

  assert.ok(socket instanceof net.Socket);

  assert.strictEqual(socket.writable, true);
  assert.strictEqual(socket.readable, true);
  assert.strictEqual(typeof socket.address(), 'object');

  socket.setTimeout(987);
  assert.strictEqual(session[kTimeout]._idleTimeout, 987);

  common.expectsError(() => socket.destroy, errMsg);
  common.expectsError(() => socket.emit, errMsg);
  common.expectsError(() => socket.end, errMsg);
  common.expectsError(() => socket.pause, errMsg);
  common.expectsError(() => socket.read, errMsg);
  common.expectsError(() => socket.resume, errMsg);
  common.expectsError(() => socket.write, errMsg);

  common.expectsError(() => (socket.destroy = undefined), errMsg);
  common.expectsError(() => (socket.emit = undefined), errMsg);
  common.expectsError(() => (socket.end = undefined), errMsg);
  common.expectsError(() => (socket.pause = undefined), errMsg);
  common.expectsError(() => (socket.read = undefined), errMsg);
  common.expectsError(() => (socket.resume = undefined), errMsg);
  common.expectsError(() => (socket.write = undefined), errMsg);

  assert.doesNotThrow(() => (socket.on = socket.on));
  assert.doesNotThrow(() => (socket.once = socket.once));

  stream.respond();

  socket.writable = 0;
  socket.readable = 0;
  assert.strictEqual(socket.writable, 0);
  assert.strictEqual(socket.readable, 0);

  stream.end();

  stream.session.on('close', common.mustCall(() => {
    assert.strictEqual(session.socket, undefined);
  }));
}));

server.listen(0, common.mustCall(function() {
  const port = server.address().port;
  const url = `http://localhost:${port}`;
  const client = h2.connect(url, common.mustCall(() => {
    const request = client.request();
    request.on('end', common.mustCall(() => {
      client.close();
      server.close();
    }));
    request.resume();
  }));
}));
