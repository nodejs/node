'use strict';

require('../common');
const assert = require('assert');
const timers = require('timers');

[
  {},
  [],
  'foo',
  () => { },
  Symbol('foo'),
].forEach((val) => {
  assert.throws(
    () => timers.enroll({}, val),
    {
      code: 'ERR_INVALID_ARG_TYPE',
      name: 'TypeError'
    }
  );
});

[
  -1,
  Infinity,
  NaN,
].forEach((val) => {
  assert.throws(
    () => timers.enroll({}, val),
    {
      code: 'ERR_OUT_OF_RANGE',
      name: 'RangeError',
    }
  );
});
