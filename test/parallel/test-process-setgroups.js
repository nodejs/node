'use strict';
const common = require('../common');
const assert = require('assert');

if (common.isWindows || !common.isMainThread) {
  assert.strictEqual(process.setgroups, undefined);
  return;
}

assert.throws(
  () => {
    process.setgroups();
  },
  {
    name: 'TypeError',
    message: 'argument 1 must be an array'
  }
);

assert.throws(
  () => {
    process.setgroups([1, -1]);
  },
  {
    name: 'Error',
    message: 'group name not found'
  }
);

[undefined, null, true, {}, [], () => {}].forEach((val) => {
  assert.throws(
    () => {
      process.setgroups([val]);
    },
    {
      name: 'Error',
      message: 'group name not found'
    }
  );
});

assert.throws(() => {
  process.setgroups([1, 'fhqwhgadshgnsdhjsdbkhsdabkfabkveyb']);
}, {
  name: 'Error',
  message: 'group name not found'
});
