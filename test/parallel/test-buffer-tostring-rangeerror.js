'use strict';

const common = require('../common');

// This test ensures that Node.js throws an Error when trying to convert a
// large buffer into a string.
// Regression test for https://github.com/nodejs/node/issues/649.

if (!common.enoughTestMem) {
  common.skip('skipped due to memory requirements');
}

const assert = require('assert');
const {
  Buffer,
  constants: {
    MAX_STRING_LENGTH,
  },
} = require('buffer');

const len = MAX_STRING_LENGTH + 1;
const message = {
  code: 'ERR_STRING_TOO_LONG',
  name: 'Error',
};

function test(getBuffer) {
  let buf;
  try {
    buf = getBuffer();
  } catch (e) {
    // If the buffer allocation fails, we skip the test.
    if (e.code === 'ERR_MEMORY_ALLOCATION_FAILED' || /Array buffer allocation failed/.test(e.message)) {
      return;
    }
  }
  assert.throws(() => { buf.toString('utf8'); }, message);
}

test(() => Buffer(len));
test(() => Buffer.alloc(len));
test(() => Buffer.allocUnsafe(len));
test(() => Buffer.allocUnsafeSlow(len));
