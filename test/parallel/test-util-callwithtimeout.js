'use strict';

require('../common');
const assert = require('assert');
const util = require('util');

[
  true,
  false,
  'hi',
  {},
  [],
  () => {},
].forEach((t) => {
  assert.throws(() => {
    util.callWithTimeout(t, () => {});
  }, TypeError);
});

[-1, 0, Infinity, NaN, 2 ** 33].forEach((n) => {
  assert.throws(() => {
    util.callWithTimeout(n, () => {});
  }, RangeError);
});

[
  -1,
  0,
  1,
  true,
  false,
  'hi',
  {},
  [],
].forEach((t) => {
  assert.throws(() => {
    util.callWithTimeout(1, t);
  }, TypeError);
});

assert.throws(() => {
  util.callWithTimeout(10, () => {
    while (true) {}
  });
}, {
  code: 'ERR_SCRIPT_EXECUTION_TIMEOUT',
});
