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

const date = new Date();

common.refreshTmpDir();

const file = path.join(common.tmpDir, 'a.js');
closeSync(openSync(file, 'w'));

// futimes
async function test(file) {
  const fd = await fs.open(file, 'w');
  await fs.futimes(fd, date, date);
  await fs.close(fd);
}

test(file)
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

// utimes
const goodurl = new URL(`file://${file}`);
const badurl = new URL(`file://${file}-NO`);
const nullurl = new URL(`file://${file}-%00NO`);
const invalidurl = new URL(`http://${file}`);

fs.utimes(file, date, date)
  .then(common.mustCall())
  .catch(common.mustNotCall());

fs.utimes(Buffer.from(file), date, date)
  .then(common.mustCall())
  .catch(common.mustNotCall());

fs.utimes(goodurl, date, date)
  .then(common.mustCall())
  .catch(common.mustNotCall());

fs.utimes(file, date.valueOf(), date.valueOf())
  .then(common.mustCall())
  .catch(common.mustNotCall());

fs.utimes(Buffer.from(file), date.valueOf(), date.valueOf())
  .then(common.mustCall())
  .catch(common.mustNotCall());

fs.utimes(goodurl, date.valueOf(), date.valueOf())
  .then(common.mustCall())
  .catch(common.mustNotCall());

fs.utimes(badurl, date, date)
  .then(common.mustNotCall())
  .catch(common.expectsError({
    code: 'ENOENT',
    type: Error,
    message: `ENOENT: no such file or directory, utime '${file}-NO'`
  }));

fs.utimes(nullurl, date, date)
  .then(common.mustNotCall())
  .catch(common.expectsError({
    code: 'ERR_INVALID_ARG_TYPE',
    type: Error,
    message: 'The "path" argument must be of type string without null bytes. ' +
             'Received type string'
  }));

fs.utimes(invalidurl, date, date)
  .then(common.mustNotCall())
  .catch(common.expectsError({
    code: 'ERR_INVALID_URL_SCHEME',
    type: TypeError,
    message: 'The URL must be of scheme file'
  }));

[1, {}, [], Infinity, null, undefined, true].forEach((i) => {
  fs.utimes(i, date, date)
    .then(common.mustNotCall())
    .catch(common.expectsError({
      code: 'ERR_INVALID_ARG_TYPE',
      type: TypeError,
      message: 'The "path" argument must be one of type string, Buffer, or URL'
    }));
});
