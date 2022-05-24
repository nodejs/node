'use strict';
const common = require('../common');
const assert = require('assert');

if (common.isWindows) {
  assert.strictEqual(process.initgroups, undefined);
  return;
}

if (!common.isMainThread)
  return;

[undefined, null, true, {}, [], () => {}].forEach((val) => {
  assert.throws(
    () => {
      process.initgroups(val);
    },
    {
      code: 'ERR_INVALID_ARG_TYPE',
      name: 'TypeError',
    }
  );
});

[undefined, null, true, {}, [], () => {}].forEach((val) => {
  assert.throws(
    () => {
      process.initgroups('foo', val);
    },
    {
      code: 'ERR_INVALID_ARG_TYPE',
      name: 'TypeError',
    }
  );
});

assert.throws(
  () => {
    process.initgroups(
      'fhqwhgadshgnsdhjsdbkhsdabkfabkveyb',
      'fhqwhgadshgnsdhjsdbkhsdabkfabkveyb'
    );
  },
  {
    code: 'ERR_UNKNOWN_CREDENTIAL',
  }
);
