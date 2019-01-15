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
      name: 'TypeError [ERR_INVALID_ARG_TYPE]',
      message:
        'The "user" argument must be ' +
        'one of type number or string. ' +
        `Received type ${typeof val}`
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
      name: 'TypeError [ERR_INVALID_ARG_TYPE]',
      message:
        'The "extraGroup" argument must be ' +
        'one of type number or string. ' +
        `Received type ${typeof val}`
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
    message:
      'Group identifier does not exist: fhqwhgadshgnsdhjsdbkhsdabkfabkveyb'
  }
);
