'use strict';

const common = require('../common');
const assert = require('assert');
const fs = require('fs');

[false, 1, {}, [], null, undefined].forEach((i) => {
  assert.throws(
    () => fs.chown(i, 1, 1, common.mustNotCall()),
    {
      code: 'ERR_INVALID_ARG_TYPE',
      name: 'TypeError'
    }
  );
  assert.throws(
    () => fs.chownSync(i, 1, 1),
    {
      code: 'ERR_INVALID_ARG_TYPE',
      name: 'TypeError'
    }
  );
});

[false, 'test', {}, [], null, undefined].forEach((i) => {
  assert.throws(
    () => fs.chown('not_a_file_that_exists', i, 1, common.mustNotCall()),
    {
      code: 'ERR_INVALID_ARG_TYPE',
      name: 'TypeError'
    }
  );
  assert.throws(
    () => fs.chown('not_a_file_that_exists', 1, i, common.mustNotCall()),
    {
      code: 'ERR_INVALID_ARG_TYPE',
      name: 'TypeError'
    }
  );
  assert.throws(
    () => fs.chownSync('not_a_file_that_exists', i, 1),
    {
      code: 'ERR_INVALID_ARG_TYPE',
      name: 'TypeError'
    }
  );
  assert.throws(
    () => fs.chownSync('not_a_file_that_exists', 1, i),
    {
      code: 'ERR_INVALID_ARG_TYPE',
      name: 'TypeError'
    }
  );
});
