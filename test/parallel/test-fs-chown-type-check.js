'use strict';

const common = require('../common');
const fs = require('fs');

[false, 1, {}, [], null, undefined].forEach((i) => {
  common.expectsError(
    () => fs.chown(i, 1, 1, common.mustNotCall()),
    {
      code: 'ERR_INVALID_ARG_TYPE',
      type: TypeError
    }
  );
  common.expectsError(
    () => fs.chownSync(i, 1, 1),
    {
      code: 'ERR_INVALID_ARG_TYPE',
      type: TypeError
    }
  );
});

[false, 'test', {}, [], null, undefined].forEach((i) => {
  common.expectsError(
    () => fs.chown('not_a_file_that_exists', i, 1, common.mustNotCall()),
    {
      code: 'ERR_INVALID_ARG_TYPE',
      type: TypeError
    }
  );
  common.expectsError(
    () => fs.chown('not_a_file_that_exists', 1, i, common.mustNotCall()),
    {
      code: 'ERR_INVALID_ARG_TYPE',
      type: TypeError
    }
  );
  common.expectsError(
    () => fs.chownSync('not_a_file_that_exists', i, 1),
    {
      code: 'ERR_INVALID_ARG_TYPE',
      type: TypeError
    }
  );
  common.expectsError(
    () => fs.chownSync('not_a_file_that_exists', 1, i),
    {
      code: 'ERR_INVALID_ARG_TYPE',
      type: TypeError
    }
  );
});
