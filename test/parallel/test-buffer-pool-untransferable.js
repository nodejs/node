'use strict';
require('../common');
const assert = require('assert');
const { MessageChannel } = require('worker_threads');

// Make sure that the pools used by the Buffer implementation are not
// transferable.
// Refs: https://github.com/nodejs/node/issues/32752

const a = Buffer.from('hello world');
const b = Buffer.from('hello world');
assert.strictEqual(a.buffer, b.buffer);
const length = a.length;

const { port1 } = new MessageChannel();
assert.throws(() => port1.postMessage(a, [ a.buffer ]), {
  code: 25,
  name: 'DataCloneError',
});

// Verify that the pool ArrayBuffer has not actually been transferred:
assert.strictEqual(a.buffer, b.buffer);
assert.strictEqual(a.length, length);

if (typeof ArrayBuffer.prototype.transfer !== 'function')
  common.skip('ArrayBuffer.prototype.transfer is not available');

// Regression test for https://github.com/nodejs/node/issues/61362
const base64 = 'aGVsbG8='; // "hello"
const buf = Buffer.from(base64, 'base64');
buf.buffer.transfer();
assert.strictEqual(buf.buffer.byteLength, 0);
assert.doesNotThrow(() => {
  const out = Buffer.from(base64, 'base64');
  assert.strictEqual(out.toString(), 'hello');
});
