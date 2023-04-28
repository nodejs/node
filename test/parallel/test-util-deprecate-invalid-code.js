'use strict';

const common = require('../common');
const assert = require('assert');
const util = require('util');

[1, true, false, null, {}].forEach((notString) => {
  assert.throws(() => util.deprecate(() => {}, 'message', notString), {
    code: 'ERR_INVALID_ARG_TYPE',
    name: 'TypeError',
    message: 'The "code" argument must be of type string.' +
             common.invalidArgTypeHelper(notString)
  });
});
