'use strict';

const common = require('../common');
const fs = require('fs');

['', false, null, undefined, {}, []].forEach((i) => {
  common.expectsError(
    () => fs.fchown(i),
    {
      code: 'ERR_INVALID_ARG_TYPE',
      type: TypeError,
      message: 'The "fd" argument must be of type integer'
    }
  );
  common.expectsError(
    () => fs.fchownSync(i),
    {
      code: 'ERR_INVALID_ARG_TYPE',
      type: TypeError,
      message: 'The "fd" argument must be of type integer'
    }
  );

  common.expectsError(
    () => fs.fchown(1, i),
    {
      code: 'ERR_INVALID_ARG_TYPE',
      type: TypeError,
      message: 'The "uid" argument must be of type integer'
    }
  );
  common.expectsError(
    () => fs.fchownSync(1, i),
    {
      code: 'ERR_INVALID_ARG_TYPE',
      type: TypeError,
      message: 'The "uid" argument must be of type integer'
    }
  );

  common.expectsError(
    () => fs.fchown(1, 1, i),
    {
      code: 'ERR_INVALID_ARG_TYPE',
      type: TypeError,
      message: 'The "gid" argument must be of type integer'
    }
  );
  common.expectsError(
    () => fs.fchownSync(1, 1, i),
    {
      code: 'ERR_INVALID_ARG_TYPE',
      type: TypeError,
      message: 'The "gid" argument must be of type integer'
    }
  );
});
