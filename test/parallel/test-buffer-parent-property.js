'use strict';

// Fix for https://github.com/nodejs/node/issues/8266
//
// Zero length Buffer objects should expose the `buffer` property of the
// TypedArrays, via the `parent` property.
require('../common');
const assert = require('assert');

// If the length of the buffer object is zero
assert((new Buffer(0)).parent instanceof ArrayBuffer);

// If the length of the buffer object is equal to the underlying ArrayBuffer
assert((new Buffer(Buffer.poolSize)).parent instanceof ArrayBuffer);

// Same as the previous test, but with user created buffer
const arrayBuffer = new ArrayBuffer(0);
assert.strictEqual(new Buffer(arrayBuffer).parent, arrayBuffer);
assert.strictEqual(new Buffer(arrayBuffer).buffer, arrayBuffer);
assert.strictEqual(Buffer.from(arrayBuffer).parent, arrayBuffer);
assert.strictEqual(Buffer.from(arrayBuffer).buffer, arrayBuffer);
