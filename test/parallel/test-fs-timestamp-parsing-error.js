'use strict';
require('../common');
const assert = require('assert');
const fs = require('fs');

[undefined, null, []].forEach((input) => {
  assert.throws(() => fs._toUnixTimestamp(input),
                new RegExp('^Error: Cannot parse time: ' + input + '$'));
});

assert.throws(() => fs._toUnixTimestamp({}),
              /^Error: Cannot parse time: \[object Object\]$/);

[1, '1', Date.now(), -1, '-1', Infinity].forEach((input) => {
  assert.doesNotThrow(() => fs._toUnixTimestamp(input));
});
