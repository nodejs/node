'use strict';

const common = require('../../common');
const assert = require('assert');
const {
  makeBufferInNewContext
} = require(`./build/${common.buildType}/binding`);

// Because the `Buffer` function and its protoype property only (currently)
// exist in a Node.js instance’s main context, trying to create buffers from
// another context throws an exception.

try {
  makeBufferInNewContext();
} catch (exception) {
  assert.strictEqual(exception.constructor.name, 'Error');
  assert(!(exception.constructor instanceof Error));

  assert.strictEqual(exception.code, 'ERR_BUFFER_CONTEXT_NOT_AVAILABLE');
  assert.strictEqual(exception.message,
                     'Buffer is not available for the current Context');
}
