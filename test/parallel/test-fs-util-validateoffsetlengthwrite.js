// Flags: --expose-internals
'use strict';

const common = require('../common');

const { validateOffsetLengthWrite } = require('internal/fs/utils');
const { kMaxLength } = require('buffer');

// RangeError when offset > byteLength
{
  const offset = 100;
  const length = 100;
  const byteLength = 50;
  common.expectsError(
    () => validateOffsetLengthWrite(offset, length, byteLength),
    {
      code: 'ERR_OUT_OF_RANGE',
      type: RangeError,
      message: 'The value of "offset" is out of range. ' +
               `It must be <= ${byteLength}. Received ${offset}`
    }
  );
}

// RangeError when byteLength > kMaxLength, and length > kMaxLength - offset .
{
  const offset = kMaxLength;
  const length = 100;
  const byteLength = kMaxLength + 1;
  common.expectsError(
    () => validateOffsetLengthWrite(offset, length, byteLength),
    {
      code: 'ERR_OUT_OF_RANGE',
      type: RangeError,
      message: 'The value of "length" is out of range. ' +
               `It must be <= ${kMaxLength - offset}. Received ${length}`
    }
  );
}

// RangeError when byteLength < kMaxLength, and length > byteLength - offset .
{
  const offset = kMaxLength - 150;
  const length = 200;
  const byteLength = kMaxLength - 100;
  common.expectsError(
    () => validateOffsetLengthWrite(offset, length, byteLength),
    {
      code: 'ERR_OUT_OF_RANGE',
      type: RangeError,
      message: 'The value of "length" is out of range. ' +
               `It must be <= ${byteLength - offset}. Received ${length}`
    }
  );
}
