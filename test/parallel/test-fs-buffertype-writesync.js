'use strict';
require('../common');

// This test ensures that writeSync throws for invalid data input.

const assert = require('assert');
const fs = require('fs');

const testCases = [
  true, false, 0, 1, Infinity, () => {}, {}, [], undefined, null,
];
for (const value of testCases) {
  assert.throws(
    () => fs.writeSync(1, value),
    { message: /"buffer"/, code: 'ERR_INVALID_ARG_TYPE' }
  );
}
