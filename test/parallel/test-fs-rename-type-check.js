'use strict';

const common = require('../common');
const fs = require('fs');

[false, 1, [], {}, null, undefined].forEach((i) => {
  common.expectsError(
    () => fs.rename(i, 'does-not-exist', common.mustNotCall()),
    {
      code: 'ERR_INVALID_ARG_TYPE',
      type: TypeError,
      message:
        'The "oldPath" argument must be one of type string, Buffer, or URL'
    }
  );
  common.expectsError(
    () => fs.rename('does-not-exist', i, common.mustNotCall()),
    {
      code: 'ERR_INVALID_ARG_TYPE',
      type: TypeError,
      message:
        'The "newPath" argument must be one of type string, Buffer, or URL'
    }
  );
  common.expectsError(
    () => fs.renameSync(i, 'does-not-exist'),
    {
      code: 'ERR_INVALID_ARG_TYPE',
      type: TypeError,
      message:
        'The "oldPath" argument must be one of type string, Buffer, or URL'
    }
  );
  common.expectsError(
    () => fs.renameSync('does-not-exist', i),
    {
      code: 'ERR_INVALID_ARG_TYPE',
      type: TypeError,
      message:
        'The "newPath" argument must be one of type string, Buffer, or URL'
    }
  );
});
