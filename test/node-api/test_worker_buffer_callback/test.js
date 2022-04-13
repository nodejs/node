'use strict';
const common = require('../../common');
const assert = require('assert');
const { MessageChannel } = require('worker_threads');
const { buffer } = require(`./build/${common.buildType}/binding`);

// Test that buffers allocated with a free callback through our APIs are not
// transferred.

const { port1 } = new MessageChannel();
const origByteLength = buffer.byteLength;
port1.postMessage(buffer, [buffer]);

assert.strictEqual(buffer.byteLength, origByteLength);
assert.notStrictEqual(buffer.byteLength, 0);
