'use strict';
// Flags: --expose-gc

const common = require('../../common');
const binding = require(`./build/${common.buildType}/test_buffer`);
const assert = require('assert');

assert.strictEqual(binding.newBuffer().toString(), binding.theText,
                   'buffer returned by newBuffer() has wrong contents');
assert.strictEqual(binding.newExternalBuffer().toString(), binding.theText,
                   'buffer returned by newExternalBuffer() has wrong contents');
console.log('gc1');
global.gc();
assert.strictEqual(binding.getDeleterCallCount(), 1, 'deleter was not called');
assert.strictEqual(binding.copyBuffer().toString(), binding.theText,
                   'buffer returned by copyBuffer() has wrong contents');

let buffer = binding.staticBuffer();
assert.strictEqual(binding.bufferHasInstance(buffer), true,
                   'buffer type checking fails');
assert.strictEqual(binding.bufferInfo(buffer), true, 'buffer data is accurate');
buffer = null;
global.gc();
console.log('gc2');
assert.strictEqual(binding.getDeleterCallCount(), 2, 'deleter was not called');
