'use strict';
require('../common');
const fs = require('fs');
const assert = require('assert');

[Infinity, -Infinity, NaN].forEach((input) => {
  assert.throws(() => fs._toUnixTimestamp(input),
                new RegExp('^Error: Cannot parse time: ' + input + '$'));
});

assert.throws(() => fs._toUnixTimestamp({}),
              /^Error: Cannot parse time: \[object Object\]$/);

const okInputs = [1, -1, '1', '-1', Date.now()];
okInputs.forEach((input) => {
  assert.doesNotThrow(() => fs._toUnixTimestamp(input));
});
