'use strict';

const common = require('../common');
const assert = require('assert');
const fs = require('fs');

[false, 1, [], {}, null, undefined].forEach((i) => {
  assert.throws(
    () => fs.rmdir(i, common.mustNotCall()),
    {
      code: 'ERR_INVALID_ARG_TYPE',
      name: 'TypeError'
    }
  );
  assert.throws(
    () => fs.rmdirSync(i),
    {
      code: 'ERR_INVALID_ARG_TYPE',
      name: 'TypeError'
    }
  );
});
