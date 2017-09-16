'use strict';

const common = require('../common');
const {
  async: fs,
  closeSync,
  openSync
} = require('fs');
const path = require('path');

common.crashOnUnhandledRejection();

common.refreshTmpDir();

const file = path.join(common.tmpDir, 'a.js');
closeSync(openSync(file, 'w'));

async function test(method, file) {
  const fd = await fs.open(file, 'w');
  await method(fd);
}

test(fs.fdatasync, file)
  .then(common.mustCall())
  .catch(common.mustNotCall());

test(fs.fsync, file)
  .then(common.mustCall())
  .catch(common.mustNotCall());

[-1, {}, [], '-1', null, undefined, Infinity].forEach((i) => {
  fs.fdatasync(i)
    .then(common.mustNotCall)
    .catch(common.expectsError({
      code: 'ERR_INVALID_ARG_TYPE',
      type: TypeError,
      message: 'The "fd" argument must be of type unsigned integer'
    }));
  fs.fsync(i)
    .then(common.mustNotCall)
    .catch(common.expectsError({
      code: 'ERR_INVALID_ARG_TYPE',
      type: TypeError,
      message: 'The "fd" argument must be of type unsigned integer'
    }));
});
