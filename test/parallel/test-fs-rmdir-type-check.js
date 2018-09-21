'use strict';

const common = require('../common');
const fs = require('fs');

[false, 1, [], {}, null, undefined].forEach((i) => {
  common.expectsError(
    () => fs.rmdir(i, common.mustNotCall()),
    {
      code: 'ERR_INVALID_ARG_TYPE',
      type: TypeError
    }
  );
  common.expectsError(
    () => fs.rmdirSync(i),
    {
      code: 'ERR_INVALID_ARG_TYPE',
      type: TypeError
    }
  );
});
