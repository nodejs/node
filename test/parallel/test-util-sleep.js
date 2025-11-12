// Flags: --expose-internals
'use strict';
require('../common');
const assert = require('assert');
const { sleep } = require('internal/util');

[undefined, null, '', {}, true, false].forEach((value) => {
  assert.throws(
    () => { sleep(value); },
    /The "msec" argument must be of type number/
  );
});

[-1, 3.14, NaN, 4294967296].forEach((value) => {
  assert.throws(
    () => { sleep(value); },
    /The value of "msec" is out of range/
  );
});
