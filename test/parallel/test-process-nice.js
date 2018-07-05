'use strict';
const common = require('../common');
const assert = require('assert');

if (common.isWindows || !common.isMainThread) {
  assert.strictEqual(process.nice, undefined);
  return;
}

assert.throws(
  () => {
    process.nice(NaN);
  },
  {
    code: 'ERR_OUT_OF_RANGE',
    name: 'RangeError [ERR_OUT_OF_RANGE]',
    message: 'The value of "nice" is out of range. It must be ' +
             'an integer. Received NaN'
  }
);

assert.throws(
  () => {
    process.nice(Infinity);
  },
  {
    code: 'ERR_OUT_OF_RANGE',
    name: 'RangeError [ERR_OUT_OF_RANGE]',
    message: 'The value of "nice" is out of range. It must be ' +
             'an integer. Received Infinity'
  }
);

[null, false, true, {}, [], () => {}, 'text'].forEach((val) => {
  assert.throws(
    () => {
      process.nice(val);
    },
    {
      code: 'ERR_INVALID_ARG_TYPE',
      name: 'TypeError [ERR_INVALID_ARG_TYPE]',
      message: 'The "nice" argument must be ' +
               'one of type number or undefined. ' +
               `Received type ${typeof val}`
    }
  );
});
