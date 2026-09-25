'use strict';
const common = require('../common');

// Buffer with size > INT32_MAX
common.skipIf32Bits();

const assert = require('assert');
const { StringDecoder } = require('node:string_decoder');
const kStringMaxLength = require('buffer').constants.MAX_STRING_LENGTH;

const size = 2 ** 31;

const stringTooLongError = {
  message: `Cannot create a string longer than 0x${kStringMaxLength.toString(16)}` +
    ' characters',
  code: 'ERR_STRING_TOO_LONG',
  name: 'Error',
};

let largeBuffer;

try {
  largeBuffer = Buffer.allocUnsafe(size);
} catch (e) {
  if (
    e.code === 'ERR_MEMORY_ALLOCATION_FAILED' ||
    /Array buffer allocation failed/.test(e.message)
  ) {
    common.skip('insufficient space for Buffer.allocUnsafe');
  }

  throw e;
}

const decoder = new StringDecoder('utf8');
assert.throws(() => decoder.write(largeBuffer), stringTooLongError);
