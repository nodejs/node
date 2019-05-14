'use strict';

const common = require('../common');
const fs = require('fs');

[false, 1, {}, [], null, undefined].forEach((i) => {
  common.expectsError(
    () => fs.readlink(i, common.mustNotCall()),
    {
      code: 'ERR_INVALID_ARG_TYPE',
      type: TypeError
    }
  );
  common.expectsError(
    () => fs.readlinkSync(i),
    {
      code: 'ERR_INVALID_ARG_TYPE',
      type: TypeError
    }
  );
});
