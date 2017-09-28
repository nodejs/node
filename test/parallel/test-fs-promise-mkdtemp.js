'use strict';

const common = require('../common');
const {
  async: fs
} = require('fs');

common.crashOnUnhandledRejection();

common.refreshTmpDir();

fs.mkdtemp(`${common.tmpDir}-`)
  .then(fs.rmdir)
  .then(common.mustCall())
  .catch(common.mustNotCall());

fs.mkdtemp(`${common.tmpDir}-`, 'buffer')
  .then(fs.rmdir)
  .then(common.mustCall())
  .catch(common.mustNotCall());

fs.mkdtemp(`${common.tmpDir}-`, { encoding: 'buffer' })
  .then(fs.rmdir)
  .then(common.mustCall())
  .catch(common.mustNotCall());

[-1, null, undefined, {}, [], Infinity, true].forEach((i) => {
  fs.mkdtemp(i)
    .then(common.mustNotCall())
    .catch(common.expectsError({
      code: 'ERR_INVALID_ARG_TYPE',
      type: TypeError,
      message: 'The "prefix" argument must be of type string'
    }));
});
