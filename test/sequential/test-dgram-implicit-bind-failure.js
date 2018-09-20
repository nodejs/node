// Flags: --expose-internals
'use strict';
const common = require('../common');
const assert = require('assert');
const dgram = require('dgram');
const dns = require('dns');
const { kStateSymbol } = require('internal/dgram');

// Monkey patch dns.lookup() so that it always fails.
dns.lookup = function(address, family, callback) {
  process.nextTick(() => { callback(new Error('fake DNS')); });
};

const socket = dgram.createSocket('udp4');
let dnsFailures = 0;
let sendFailures = 0;

process.on('exit', () => {
  assert.strictEqual(dnsFailures, 3);
  assert.strictEqual(sendFailures, 3);
});

socket.on('error', (err) => {
  if (/^Error: fake DNS$/.test(err)) {
    // The DNS lookup should fail since it is monkey patched. At that point in
    // time, the send queue should be populated with the send() operation. There
    // should also be two listeners - this function and the dgram internal one
    // time error handler.
    dnsFailures++;
    assert(Array.isArray(socket[kStateSymbol].queue));
    assert.strictEqual(socket[kStateSymbol].queue.length, 1);
    assert.strictEqual(socket.listenerCount('error'), 2);
    return;
  }

  if (err.code === 'ERR_SOCKET_CANNOT_SEND') {
    // On error, the queue should be destroyed and this function should be
    // the only listener.
    sendFailures++;
    assert.strictEqual(socket[kStateSymbol].queue, undefined);
    assert.strictEqual(socket.listenerCount('error'), 1);
    return;
  }

  assert.fail(`Unexpected error: ${err}`);
});

// Initiate a few send() operations, which will fail.
socket.send('foobar', common.PORT, 'localhost');

process.nextTick(() => {
  socket.send('foobar', common.PORT, 'localhost');
});

setImmediate(() => {
  socket.send('foobar', common.PORT, 'localhost');
});
