'use strict';

const assert = require('assert');
const util = require('util');

[1, true, false, null, {}].forEach((notString) => {
  assert.throws(() => util.deprecate(() => {}, 'message', notString), {
    code: 'ERR_INVALID_ARG_TYPE',
    name: 'TypeError',
  });
});
