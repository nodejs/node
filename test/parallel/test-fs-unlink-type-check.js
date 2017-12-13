'use strict';

const common = require('../common');
const fs = require('fs');

[false, 1, {}, [], null, undefined].forEach((i) => {
  common.expectsError(
    () => fs.unlink(i, common.mustNotCall()),
    {
      code: 'ERR_INVALID_ARG_TYPE',
      type: TypeError
    }
  );
  common.expectsError(
    () => fs.unlinkSync(i),
    {
      code: 'ERR_INVALID_ARG_TYPE',
      type: TypeError
    }
  );
});
