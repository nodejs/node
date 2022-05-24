'use strict';

const common = require('../common');
const assert = require('assert');
const fs = require('fs');

[false, 1, [], {}, null, undefined].forEach((input) => {
  assert.throws(
    () => fs.rename(input, 'does-not-exist', common.mustNotCall()),
    {
      code: 'ERR_INVALID_ARG_TYPE',
      name: 'TypeError',
    }
  );
  assert.throws(
    () => fs.rename('does-not-exist', input, common.mustNotCall()),
    {
      code: 'ERR_INVALID_ARG_TYPE',
      name: 'TypeError',
    }
  );
  assert.throws(
    () => fs.renameSync(input, 'does-not-exist'),
    {
      code: 'ERR_INVALID_ARG_TYPE',
      name: 'TypeError',
    }
  );
  assert.throws(
    () => fs.renameSync('does-not-exist', input),
    {
      code: 'ERR_INVALID_ARG_TYPE',
      name: 'TypeError',
    }
  );
});
