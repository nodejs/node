'use strict';

const common = require('../common');
if (!common.isMainThread)
  common.skip('process.chdir is not available in Workers');

common.expectsError(
  () => {
    process.chdir('does-not-exist');
  },
  {
    type: Error,
    code: 'ENOENT',
    message: /ENOENT: no such file or directory, chdir .+ -> 'does-not-exist'/,
    path: process.cwd(),
    syscall: 'chdir',
    dest: 'does-not-exist'
  }
);
