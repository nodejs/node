'use strict';
require('../common');
const assert = require('assert');

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
