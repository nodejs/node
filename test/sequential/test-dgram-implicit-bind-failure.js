// Flags: --expose-internals
'use strict';
const common = require('../common');
const assert = require('assert');
const EventEmitter = require('events');
const dgram = require('dgram');
const dns = require('dns');
const { kStateSymbol } = require('internal/dgram');
const mockError = new Error('fake DNS');

// Monkey patch dns.lookup() so that it always fails.
dns.lookup = function(address, family, callback) {
  process.nextTick(() => { callback(mockError); });
};

const socket = dgram.createSocket('udp4');

socket.on(EventEmitter.errorMonitor, common.mustCall((err) => {
  // The DNS lookup should fail since it is monkey patched. At that point in
  // time, the send queue should be populated with the send() operation.
  assert.strictEqual(err, mockError);
  assert(Array.isArray(socket[kStateSymbol].queue));
  assert.strictEqual(socket[kStateSymbol].queue.length, 1);
}, 3));

socket.on('error', common.mustCall((err) => {
  assert.strictEqual(err, mockError);
  assert.strictEqual(socket[kStateSymbol].queue, undefined);
}, 3));

// Initiate a few send() operations, which will fail.
socket.send('foobar', common.PORT, 'localhost');

process.nextTick(() => {
  socket.send('foobar', common.PORT, 'localhost');
});

setImmediate(() => {
  socket.send('foobar', common.PORT, 'localhost');
});
