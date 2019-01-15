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
    name: 'TypeError [ERR_INVALID_ARG_TYPE]',
    message: 'The "groups" argument must be of type Array. ' +
             'Received type undefined'
  }
);

assert.throws(
  () => {
    process.setgroups([1, -1]);
  },
  {
    code: 'ERR_OUT_OF_RANGE',
    name: 'RangeError [ERR_OUT_OF_RANGE]',
    message: 'The value of "groups[1]" is out of range. ' +
              'It must be >= 0 && < 4294967296. Received -1'
  }
);

[undefined, null, true, {}, [], () => {}].forEach((val) => {
  assert.throws(
    () => {
      process.setgroups([val]);
    },
    {
      code: 'ERR_INVALID_ARG_TYPE',
      name: 'TypeError [ERR_INVALID_ARG_TYPE]',
      message: 'The "groups[0]" argument must be ' +
               'one of type number or string. ' +
               `Received type ${typeof val}`
    }
  );
});

assert.throws(() => {
  process.setgroups([1, 'fhqwhgadshgnsdhjsdbkhsdabkfabkveyb']);
}, {
  code: 'ERR_UNKNOWN_CREDENTIAL',
  message: 'Group identifier does not exist: fhqwhgadshgnsdhjsdbkhsdabkfabkveyb'
});
