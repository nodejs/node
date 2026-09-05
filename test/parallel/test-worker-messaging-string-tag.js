'use strict';

require('../common');
const assert = require('assert');
const { MessageChannel, MessagePort, BroadcastChannel } = require('worker_threads');

const classesToBeTested = [
  MessageChannel,
  MessagePort,
  BroadcastChannel,
];

for (const cls of classesToBeTested) {
  assert.strictEqual(cls.prototype[Symbol.toStringTag], cls.name);
  assert.deepStrictEqual(
    Object.getOwnPropertyDescriptor(cls.prototype, Symbol.toStringTag),
    { configurable: true, enumerable: false, value: cls.name, writable: false }
  );
}

const channel = new MessageChannel();
assert.strictEqual(Object.prototype.toString.call(channel), '[object MessageChannel]');
assert.strictEqual(Object.prototype.toString.call(channel.port1), '[object MessagePort]');
assert.strictEqual(Object.prototype.toString.call(channel.port2), '[object MessagePort]');

const broadcast = new BroadcastChannel('test');
assert.strictEqual(Object.prototype.toString.call(broadcast), '[object BroadcastChannel]');
broadcast.close();

// Test globals
assert.strictEqual(globalThis.MessageChannel, MessageChannel);
assert.strictEqual(globalThis.MessagePort, MessagePort);
assert.strictEqual(globalThis.BroadcastChannel, BroadcastChannel);
assert.strictEqual(Object.prototype.toString.call(new globalThis.MessageChannel()), '[object MessageChannel]');
assert.strictEqual(Object.prototype.toString.call(new globalThis.MessageChannel().port1), '[object MessagePort]');
const globalBroadcast = new globalThis.BroadcastChannel('test-global');
assert.strictEqual(Object.prototype.toString.call(globalBroadcast), '[object BroadcastChannel]');
globalBroadcast.close();
