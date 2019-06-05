'use strict';
// Flags: --expose-gc

const common = require('../../common');
const binding = require(`./build/${common.buildType}/test_buffer`);
const assert = require('assert');
const setImmediatePromise = require('util').promisify(setImmediate);

(async function() {
  assert.strictEqual(binding.newBuffer().toString(), binding.theText);
  assert.strictEqual(binding.newExternalBuffer().toString(), binding.theText);
  console.log('gc1');
  global.gc();
  assert.strictEqual(binding.getDeleterCallCount(), 0);
  await setImmediatePromise();
  assert.strictEqual(binding.getDeleterCallCount(), 1);
  assert.strictEqual(binding.copyBuffer().toString(), binding.theText);

  let buffer = binding.staticBuffer();
  assert.strictEqual(binding.bufferHasInstance(buffer), true);
  assert.strictEqual(binding.bufferInfo(buffer), true);
  buffer = null;
  global.gc();
  assert.strictEqual(binding.getDeleterCallCount(), 1);
  await setImmediatePromise();
  console.log('gc2');
  assert.strictEqual(binding.getDeleterCallCount(), 2);
})().then(common.mustCall());
