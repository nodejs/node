'use strict';
const common = require('../common');
const assert = require('assert');

if (common.isWindows) {
  assert.strictEqual(process.setgroups, undefined);
  return;
}

if (!common.isMainThread)
  return;

assert.throws(
  () => {
    process.setgroups();
  },
  {
    code: 'ERR_INVALID_ARG_TYPE',
    name: 'TypeError',
  }
);

assert.throws(
  () => {
    process.setgroups([1, -1]);
  },
  {
    code: 'ERR_OUT_OF_RANGE',
    name: 'RangeError',
  }
);

[undefined, null, true, {}, [], () => {}].forEach((val) => {
  assert.throws(
    () => {
      process.setgroups([val]);
    },
    {
      code: 'ERR_INVALID_ARG_TYPE',
      name: 'TypeError',
    }
  );
});

assert.throws(() => {
  process.setgroups([1, 'fhqwhgadshgnsdhjsdbkhsdabkfabkveyb']);
}, {
  code: 'ERR_UNKNOWN_CREDENTIAL',
});
