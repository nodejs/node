'use strict';

const common = require('../common');
const {
  async: fs
} = require('fs');
const { Buffer } = require('buffer');
const { URL } = require('url');
const path = require('path');

common.crashOnUnhandledRejection();

common.refreshTmpDir();

const file = path.join(common.tmpDir, 'a.js');
//closeSync(openSync(file, 'w'));

const goodurl = new URL(`file://${file}`);
const badurl = new URL(`file://${file}-NO`);
const nullurl = new URL(`file://${file}-%00NO`);
const invalidurl = new URL(`http://${file}`);

async function test(path, mode = 'w') {
  await fs.close(await fs.open(path, mode));
}

test(file)
  .then(common.mustCall())
  .catch(common.mustNotCall());

test(Buffer.from(file))
  .then(common.mustCall())
  .catch(common.mustNotCall());

test(goodurl)
  .then(common.mustCall())
  .catch(common.mustNotCall());

test(badurl, 'r')
  .then(common.mustNotCall())
  .catch(common.expectsError({
    code: 'ENOENT',
    type: Error,
    message: `ENOENT: no such file or directory, open '${file}-NO'`
  }));

test(nullurl)
  .then(common.mustNotCall())
  .catch(common.expectsError({
    code: 'ERR_INVALID_ARG_TYPE',
    type: Error,
    message: 'The "path" argument must be of type string without null bytes. ' +
             'Received type string'
  }));

test(invalidurl)
  .then(common.mustNotCall())
  .catch(common.expectsError({
    code: 'ERR_INVALID_URL_SCHEME',
    type: TypeError,
    message: 'The URL must be of scheme file'
  }));

[1, {}, [], true, Infinity, null, undefined].forEach((i) => {
  fs.open(i, 'w')
    .then(common.mustNotCall())
    .catch(common.expectsError({
      code: 'ERR_INVALID_ARG_TYPE',
      type: TypeError,
      message: 'The "path" argument must be one of type string, Buffer, or URL'
    }));
});

[-1, {}, [], true, Infinity, null, '-1'].forEach((i) => {
  fs.open(file, i)
    .then(common.mustNotCall())
    .catch(common.expectsError({
      code: 'ERR_INVALID_ARG_TYPE',
      type: TypeError,
      message:
        'The "flags" argument must be one of type string or unsigned integer'
    }));
});

['a', -1, '-1', {}, [], true, Infinity, null, undefined].forEach((i) => {
  fs.close(i)
    .then(common.mustNotCall())
    .catch(common.expectsError({
      code: 'ERR_INVALID_ARG_TYPE',
      type: TypeError,
      message: 'The "fd" argument must be of type unsigned integer'
    }));
});
