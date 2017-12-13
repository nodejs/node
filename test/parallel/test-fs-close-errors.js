'use strict';

const common = require('../common');
const fs = require('fs');

['', false, null, undefined, {}, []].forEach((i) => {
  common.expectsError(
    () => fs.close(i),
    {
      code: 'ERR_INVALID_ARG_TYPE',
      type: TypeError,
      message: 'The "fd" argument must be of type integer'
    }
  );
  common.expectsError(
    () => fs.closeSync(i),
    {
      code: 'ERR_INVALID_ARG_TYPE',
      type: TypeError,
      message: 'The "fd" argument must be of type integer'
    }
  );
});
