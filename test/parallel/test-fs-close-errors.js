'use strict';

const common = require('../common');
const fs = require('fs');

['', {}, -1, true, Infinity, undefined, null].forEach((i) => {
  common.expectsError(
    () => fs.closeSync(i),
    {
      code: 'ERR_INVALID_ARG_TYPE',
      type: TypeError,
      message: 'The "fd" argument must be of type unsigned integer'
    }
  );

  common.expectsError(
    () => fs.close(i, common.mustNotCall()),
    {
      code: 'ERR_INVALID_ARG_TYPE',
      type: TypeError,
      message: 'The "fd" argument must be of type unsigned integer'
    }
  );
});
