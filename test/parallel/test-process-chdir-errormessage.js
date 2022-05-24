'use strict';

const common = require('../common');
if (!common.isMainThread)
  common.skip('process.chdir is not available in Workers');
const assert = require('assert');

assert.throws(
  () => {
    process.chdir('does-not-exist');
  },
  {
    name: 'Error',
    code: 'ENOENT',
    path: process.cwd(),
    syscall: 'chdir',
    dest: 'does-not-exist'
  }
);
