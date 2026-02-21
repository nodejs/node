// Flags: --expose-internals

'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const assert = require('assert');
const h2 = require('http2');
const net = require('net');
const util = require('util');

const { kTimeout } = require('internal/timers');

// Tests behavior of the proxied socket on Http2Session

const errMsg = {
  code: 'ERR_HTTP2_NO_SOCKET_MANIPULATION',
  name: 'Error',
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

  // The indentation is corrected depending on the depth.
  let inspectedTimeout = util.inspect(session[kTimeout]);
  assert(inspectedTimeout.includes('  _idlePrev: [TimersList]'));
  assert(inspectedTimeout.includes('  _idleNext: [TimersList]'));
  assert(!inspectedTimeout.includes('   _idleNext: [TimersList]'));

  inspectedTimeout = util.inspect([ session[kTimeout] ]);
  assert(inspectedTimeout.includes('    _idlePrev: [TimersList]'));
  assert(inspectedTimeout.includes('    _idleNext: [TimersList]'));
  assert(!inspectedTimeout.includes('     _idleNext: [TimersList]'));

  const inspectedTimersList = util.inspect([[ session[kTimeout]._idlePrev ]]);
  assert(inspectedTimersList.includes('      _idlePrev: [Timeout]'));
  assert(inspectedTimersList.includes('      _idleNext: [Timeout]'));
  assert(!inspectedTimersList.includes('       _idleNext: [Timeout]'));

  assert.throws(() => socket.destroy, errMsg);
  assert.throws(() => socket.emit, errMsg);
  assert.throws(() => socket.end, errMsg);
  assert.throws(() => socket.pause, errMsg);
  assert.throws(() => socket.read, errMsg);
  assert.throws(() => socket.resume, errMsg);
  assert.throws(() => socket.write, errMsg);
  assert.throws(() => socket.setEncoding, errMsg);
  assert.throws(() => socket.setKeepAlive, errMsg);
  assert.throws(() => socket.setNoDelay, errMsg);

  assert.throws(() => (socket.destroy = undefined), errMsg);
  assert.throws(() => (socket.emit = undefined), errMsg);
  assert.throws(() => (socket.end = undefined), errMsg);
  assert.throws(() => (socket.pause = undefined), errMsg);
  assert.throws(() => (socket.read = undefined), errMsg);
  assert.throws(() => (socket.resume = undefined), errMsg);
  assert.throws(() => (socket.write = undefined), errMsg);
  assert.throws(() => (socket.setEncoding = undefined), errMsg);
  assert.throws(() => (socket.setKeepAlive = undefined), errMsg);
  assert.throws(() => (socket.setNoDelay = undefined), errMsg);

  // Resetting the socket listeners to their own value should not throw.
  socket.on = socket.on;  // eslint-disable-line no-self-assign
  socket.once = socket.once;  // eslint-disable-line no-self-assign

  socket.unref();
  assert.strictEqual(socket._handle.hasRef(), false);
  socket.ref();
  assert.strictEqual(socket._handle.hasRef(), true);

  stream.respond();

  socket.writable = true;
  socket.readable = true;
  assert.strictEqual(socket.writable, true);
  assert.strictEqual(socket.readable, true);
  socket.writable = false;
  socket.readable = false;
  assert.strictEqual(socket.writable, false);
  assert.strictEqual(socket.readable, false);

  stream.end();

  // Setting socket properties sets the session properties correctly.
  const fn = () => {};
  socket.setTimeout = fn;
  assert.strictEqual(session.setTimeout, fn);

  socket.ref = fn;
  assert.strictEqual(session.ref, fn);

  socket.unref = fn;
  assert.strictEqual(session.unref, fn);

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
