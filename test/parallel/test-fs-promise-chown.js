'use strict';

const common = require('../common');
if (common.isWindows)
  common.skip('unsupported on windows');
const {
  async: fs,
  closeSync,
  openSync
} = require('fs');
const { Buffer } = require('buffer');
const { URL } = require('url');
const path = require('path');

common.crashOnUnhandledRejection();

const uid = process.getuid();
const gid = process.getgid();

common.refreshTmpDir();

const file = path.join(common.tmpDir, 'a.js');
closeSync(openSync(file, 'w'));

// fchown

async function test(file) {
  const fd = await fs.open(file, 'w');

  [file, {}, [], null, undefined, true, Infinity].forEach((i) => {
    fs.fchown(fd, i, gid)
      .then(common.mustNotCall())
      .catch(common.expectsError({
        code: 'ERR_INVALID_ARG_TYPE',
        type: TypeError,
        message: 'The "uid" argument must be of type integer'
      }));
    fs.fchown(fd, uid, i)
      .then(common.mustNotCall())
      .catch(common.expectsError({
        code: 'ERR_INVALID_ARG_TYPE',
        type: TypeError,
        message: 'The "gid" argument must be of type integer'
      }));
  });

  await fs.close(fd);
}

test(file)
  .then(common.mustCall())
  .catch(common.mustNotCall());

[-1, file, {}, [], Infinity, null, undefined, true].forEach((i) => {
  fs.fchown(i, uid, gid)
    .then(common.mustNotCall())
    .catch(common.expectsError({
      code: 'ERR_INVALID_ARG_TYPE',
      type: TypeError,
      message: 'The "fd" argument must be of type unsigned integer'
    }));
});

// chown

const goodurl = new URL(`file://${file}`);
const badurl = new URL(`file://${file}-NO`);
const nullurl = new URL(`file://${file}-%00NO`);
const invalidurl = new URL(`http://${file}`);

fs.chown(file, uid, gid)
  .then(common.mustCall())
  .catch(common.mustNotCall());

fs.chown(Buffer.from(file), uid, gid)
  .then(common.mustCall())
  .catch(common.mustNotCall());

fs.chown(goodurl, uid, gid)
  .then(common.mustCall())
  .catch(common.mustNotCall());

fs.chown(badurl, uid, gid)
  .then(common.mustNotCall())
  .catch(common.expectsError({
    code: 'ENOENT',
    type: Error,
    message: `ENOENT: no such file or directory, chown '${file}-NO'`
  }));

fs.chown(nullurl, uid, gid)
  .then(common.mustNotCall())
  .catch(common.expectsError({
    code: 'ERR_INVALID_ARG_TYPE',
    type: Error,
    message: 'The "path" argument must be of type string without null bytes. ' +
             'Received type string'
  }));

fs.chown(invalidurl, uid, gid)
  .then(common.mustNotCall())
  .catch(common.expectsError({
    code: 'ERR_INVALID_URL_SCHEME',
    type: TypeError,
    message: 'The URL must be of scheme file'
  }));

[1, {}, [], Infinity, null, undefined, true].forEach((i) => {
  fs.chown(i, uid, gid)
    .then(common.mustNotCall())
    .catch(common.expectsError({
      code: 'ERR_INVALID_ARG_TYPE',
      type: TypeError,
      message: 'The "path" argument must be one of type string, Buffer, or URL'
    }));
});

['', null, {}, [], Infinity, true, '-1'].forEach((i) => {
  fs.chown(file, i, gid)
    .then(common.mustNotCall())
    .catch(common.expectsError({
      code: 'ERR_INVALID_ARG_TYPE',
      type: TypeError,
      message: 'The "uid" argument must be of type integer'
    }));
});

['', null, {}, [], Infinity, true, '-1'].forEach((i) => {
  fs.chown(file, uid, i)
    .then(common.mustNotCall())
    .catch(common.expectsError({
      code: 'ERR_INVALID_ARG_TYPE',
      type: TypeError,
      message: 'The "gid" argument must be of type integer'
    }));
});
