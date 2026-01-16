'use strict';
// Addons: binding, binding_vtable

const { addonPath } = require('../../common/addon-test');
const assert = require('assert');
const { MessageChannel } = require('worker_threads');
const { buffer } = require(addonPath);

// Test that buffers allocated with a free callback through our APIs are not
// transferred.

const { port1 } = new MessageChannel();
const origByteLength = buffer.byteLength;
assert.throws(() => port1.postMessage(buffer, [buffer]), {
  code: 25,
  name: 'DataCloneError',
});

assert.strictEqual(buffer.byteLength, origByteLength);
assert.notStrictEqual(buffer.byteLength, 0);
