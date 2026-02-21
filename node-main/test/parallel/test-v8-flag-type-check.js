'use strict';
const common = require('../common');
const assert = require('assert');
const v8 = require('v8');

[1, undefined].forEach((value) => {
  assert.throws(
    () => v8.setFlagsFromString(value),
    {
      code: 'ERR_INVALID_ARG_TYPE',
      name: 'TypeError',
      message: 'The "flags" argument must be of type string.' +
               common.invalidArgTypeHelper(value)
    }
  );
});
