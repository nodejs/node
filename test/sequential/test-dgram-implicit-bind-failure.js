// Flags: --expose-internals
'use strict';
const common = require('../common');
const assert = require('assert');
const EventEmitter = require('events');
const dgram = require('dgram');
const { kStateSymbol } = require('internal/dgram');
const mockError = new Error('fake DNS');

const socket = dgram.createSocket('udp4');

// Fail the implicit bind by making the handle's address resolution fail. A
// literal bind address is not passed to dns.lookup(), so patching dns.lookup()
// would not be observed here.
socket[kStateSymbol].handle.lookup = function(address, callback) {
  process.nextTick(() => { callback(mockError); });
};

socket.on(EventEmitter.errorMonitor, common.mustCall((err) => {
  // The bind should fail since the lookup is monkey patched. At that point in
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
