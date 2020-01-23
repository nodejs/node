// Flags: --expose-internals
'use strict';

require('../common');

const assert = require('assert');
const { validateOffsetLengthWrite } = require('internal/fs/utils');

// Most platforms don't allow reads or writes >= 2 GB.
// See https://github.com/libuv/libuv/pull/1501.
const kIoMaxLength = 2 ** 31 - 1;

// RangeError when offset > byteLength
{
  const offset = 100;
  const length = 100;
  const byteLength = 50;
  assert.throws(
    () => validateOffsetLengthWrite(offset, length, byteLength),
    {
      code: 'ERR_OUT_OF_RANGE',
      name: 'RangeError',
      message: 'The value of "offset" is out of range. ' +
               `It must be <= ${byteLength}. Received ${offset}`
    }
  );
}

// RangeError when byteLength < kIoMaxLength, and length > byteLength - offset.
{
  const offset = kIoMaxLength - 150;
  const length = 200;
  const byteLength = kIoMaxLength - 100;
  assert.throws(
    () => validateOffsetLengthWrite(offset, length, byteLength),
    {
      code: 'ERR_OUT_OF_RANGE',
      name: 'RangeError',
      message: 'The value of "length" is out of range. ' +
               `It must be <= ${byteLength - offset}. Received ${length}`
    }
  );
}
