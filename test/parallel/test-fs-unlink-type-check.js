'use strict';

const common = require('../common');
const assert = require('assert');
const fs = require('fs');

const testCases = [false, 1, {}, [], null, undefined];
for (const i of testCases) {
  assert.throws(
    () => fs.unlink(i, common.mustNotCall()),
    {
      code: 'ERR_INVALID_ARG_TYPE',
      name: 'TypeError'
    }
  );
  assert.throws(
    () => fs.unlinkSync(i),
    {
      code: 'ERR_INVALID_ARG_TYPE',
      name: 'TypeError'
    }
  );
}
