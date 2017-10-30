'use strict';
const common = require('../common');
const v8 = require('v8');

[ 1, undefined ].forEach((i) => {
  common.expectsError(
    () => v8.setFlagsFromString(i),
    {
      code: 'ERR_INVALID_ARG_TYPE',
      type: TypeError,
      message: 'The "flags" argument must be of type string'
    }
  );
});
