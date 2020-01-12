// Flags: --expose-internals
'use strict';

require('../common');

const assert = require('assert');
const { validateOffsetLengthWrite } = require('internal/fs/utils');
const { kMaxLength } = require('buffer');

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

// RangeError when byteLength < kMaxLength, and length > byteLength - offset .
{
  const offset = kMaxLength - 150;
  const length = 200;
  const byteLength = kMaxLength - 100;
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
