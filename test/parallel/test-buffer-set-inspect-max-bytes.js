'use strict';

require('../common');
const assert = require('assert');
const buffer = require('buffer');

const rangeErrorObjs = [NaN, -1];
const typeErrorObj = 'and even this';

for (const obj of rangeErrorObjs) {
  assert.throws(
    () => buffer.INSPECT_MAX_BYTES = obj,
    {
      code: 'ERR_OUT_OF_RANGE',
      name: 'RangeError',
    }
  );

  assert.throws(
    () => buffer.INSPECT_MAX_BYTES = obj,
    {
      code: 'ERR_OUT_OF_RANGE',
      name: 'RangeError',
    }
  );
}

assert.throws(
  () => buffer.INSPECT_MAX_BYTES = typeErrorObj,
  {
    code: 'ERR_INVALID_ARG_TYPE',
    name: 'TypeError',
  }
);
