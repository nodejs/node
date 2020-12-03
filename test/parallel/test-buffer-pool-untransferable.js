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
port1.postMessage(a, [ a.buffer ]);

// Verify that the pool ArrayBuffer has not actually been transferred:
assert.strictEqual(a.buffer, b.buffer);
assert.strictEqual(a.length, length);
