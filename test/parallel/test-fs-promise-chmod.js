'use strict';

const common = require('../common');
const {
  async: fs,
  closeSync,
  openSync
} = require('fs');
const { Buffer } = require('buffer');
const { URL } = require('url');
const path = require('path');

common.crashOnUnhandledRejection();

const mode = common.isWindows ? 0o600 : 0o644;

common.refreshTmpDir();

const file = path.join(common.tmpDir, 'a.js');
closeSync(openSync(file, 'w'));

// fchmod
async function test(file, mode) {
  const fd = await fs.open(file, 'w');
  await fs.fchmod(fd, mode);
  await fs.close(fd);
}

test(file, mode)
  .then(common.mustCall())
  .catch(common.mustNotCall());

[-1, file, {}, [], Infinity, true].forEach((i) => {
  fs.fchmod(i)
    .then(common.mustNotCall())
    .catch(common.expectsError({
      code: 'ERR_INVALID_ARG_TYPE',
      type: TypeError,
      message: 'The "fd" argument must be of type unsigned integer'
    }));
});

// chmod
const goodurl = new URL(`file://${file}`);
const badurl = new URL(`file://${file}-NO`);
const nullurl = new URL(`file://${file}-%00NO`);
const invalidurl = new URL(`http://${file}`);

fs.chmod(file, mode)
  .then(common.mustCall())
  .catch(common.mustNotCall());

fs.chmod(Buffer.from(file), mode)
  .then(common.mustCall())
  .catch(common.mustNotCall());

fs.chmod(file, mode.toString(8))
  .then(common.mustCall())
  .catch(common.mustNotCall());

fs.chmod(Buffer.from(file), mode.toString(8))
  .then(common.mustCall())
  .catch(common.mustNotCall());

fs.chmod(goodurl, mode)
  .then(common.mustCall())
  .catch(common.mustNotCall());

fs.chmod(goodurl, mode.toString(8))
  .then(common.mustCall())
  .catch(common.mustNotCall());

fs.chmod(badurl, mode)
  .then(common.mustNotCall())
  .catch(common.expectsError({
    code: 'ENOENT',
    type: Error,
    message: `ENOENT: no such file or directory, chmod '${file}-NO'`
  }));

fs.chmod(nullurl, mode)
  .then(common.mustNotCall())
  .catch(common.expectsError({
    code: 'ERR_INVALID_ARG_TYPE',
    type: Error,
    message: 'The "path" argument must be of type string without null bytes. ' +
             'Received type string'
  }));

fs.chmod(invalidurl, mode)
  .then(common.mustNotCall())
  .catch(common.expectsError({
    code: 'ERR_INVALID_URL_SCHEME',
    type: TypeError,
    message: 'The URL must be of scheme file'
  }));

[1, {}, [], Infinity, null, undefined, true].forEach((i) => {
  fs.chmod(i, mode)
    .then(common.mustNotCall())
    .catch(common.expectsError({
      code: 'ERR_INVALID_ARG_TYPE',
      type: TypeError,
      message: 'The "path" argument must be one of type string, Buffer, or URL'
    }));
});

['', -1, null, {}, [], Infinity, true, '-1'].forEach((i) => {
  fs.chmod(file, i)
    .then(common.mustNotCall())
    .catch(common.expectsError({
      code: 'ERR_INVALID_ARG_TYPE',
      type: TypeError,
      message: 'The "mode" argument must be of type unsigned integer'
    }));
});
