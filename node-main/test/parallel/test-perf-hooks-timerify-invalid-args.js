// Test that timerify throws appropriate errors for invalid argument types.

'use strict';

require('../common');
const assert = require('assert');
const { timerify } = require('perf_hooks');

[1, {}, [], null, undefined, Infinity].forEach((input) => {
  assert.throws(() => timerify(input),
                {
                  code: 'ERR_INVALID_ARG_TYPE',
                  name: 'TypeError',
                  message: /The "fn" argument must be of type function/
                });
});

const asyncFunc = async (a, b = 1) => {};
const syncFunc = (a, b = 1) => {};

[1, '', {}, [], false].forEach((histogram) => {
  assert.throws(() => timerify(asyncFunc, { histogram }), {
    code: 'ERR_INVALID_ARG_TYPE'
  });
  assert.throws(() => timerify(syncFunc, { histogram }), {
    code: 'ERR_INVALID_ARG_TYPE'
  });
});
