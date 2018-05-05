'use strict';

const { expectsError } = require('../common');

expectsError(
  () => {
    process.chdir('does-not-exist');
  },
  {
    type: Error,
    code: 'ENOENT',
    message: "ENOENT: no such file or directory, chdir 'does-not-exist'",
  }
);
