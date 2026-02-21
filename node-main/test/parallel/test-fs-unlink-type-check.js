'use strict';

const common = require('../common');
const assert = require('assert');
const fs = require('fs');

[false, 1, {}, [], null, undefined].forEach((i) => {
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
});
