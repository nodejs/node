// Flags: --expose-internals
'use strict';

const common = require('../common');
const assert = require('assert');

const { myersDiff } = require('internal/assert/myers_diff');

{
  const arr1 = { length: 2 ** 31 - 1 };
  const arr2 = { length: 2 };
  const max = arr1.length + arr2.length;
  assert.throws(
    () => {
      myersDiff(arr1, arr2);
    },
    common.expectsError({
      code: 'ERR_OUT_OF_RANGE',
      name: 'RangeError',
      message: 'The value of "myersDiff input size" ' +
                'is out of range. It must be < 2^31. ' +
                `Received ${max}`
    })
  );
}
