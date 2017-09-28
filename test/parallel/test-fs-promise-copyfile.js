'use strict';

const common = require('../common');
const {
  async: fs,
  closeSync,
  openSync,
  constants
} = require('fs');
const { Buffer } = require('buffer');
const { URL } = require('url');
const path = require('path');

const { COPYFILE_EXCL, UV_FS_COPYFILE_EXCL } = constants;

common.crashOnUnhandledRejection();

common.refreshTmpDir();

const src = path.join(common.tmpDir, 'a.js');
const dest = path.join(common.tmpDir, 'b.js');
closeSync(openSync(src, 'w'));

const goodsrcurl = new URL(`file://${src}`);
const badsrcurl = new URL(`file://${src}-NO`);
const nullsrcurl = new URL(`file://${src}-%00NO`);
const invalidsrcurl = new URL(`http://${src}`);

const gooddesturl = new URL(`file://${dest}`);
const nulldesturl = new URL(`file://${dest}-%00NO`);
const invaliddesturl = new URL(`http://${dest}`);

fs.copyFile(src, dest)
  .then(common.mustCall())
  .catch(common.mustNotCall());

fs.copyFile(src, dest, COPYFILE_EXCL)
  .then(common.mustNotCall())
  .catch(common.expectsError({
    code: 'EEXIST',
    type: Error,
    message: `EEXIST: file already exists, copyfile '${src}' -> '${dest}'`
  }));

fs.copyFile(src, dest, UV_FS_COPYFILE_EXCL)
  .then(common.mustNotCall())
  .catch(common.expectsError({
    code: 'EEXIST',
    type: Error,
    message: `EEXIST: file already exists, copyfile '${src}' -> '${dest}'`
  }));

fs.copyFile(Buffer.from(src), dest)
  .then(common.mustCall())
  .catch(common.mustNotCall());

fs.copyFile(src, Buffer.from(dest))
  .then(common.mustCall())
  .catch(common.mustNotCall());

fs.copyFile(goodsrcurl, dest)
  .then(common.mustCall())
  .catch(common.mustNotCall());

fs.copyFile(goodsrcurl, gooddesturl)
  .then(common.mustCall())
  .catch(common.mustNotCall());

fs.copyFile(src, gooddesturl)
  .then(common.mustCall())
  .catch(common.mustNotCall());

fs.copyFile(Buffer.from(src), gooddesturl)
  .then(common.mustCall())
  .catch(common.mustNotCall());

fs.copyFile(badsrcurl, dest)
  .then(common.mustNotCall())
  .catch(common.expectsError({
    code: 'ENOENT',
    type: Error,
    message:
      `ENOENT: no such file or directory, copyfile '${src}-NO' -> '${dest}'`
  }));

fs.copyFile(nullsrcurl, dest)
  .then(common.mustNotCall())
  .catch(common.expectsError({
    code: 'ERR_INVALID_ARG_TYPE',
    type: Error,
    message: 'The "path" argument must be of type string without null bytes. ' +
             'Received type string'
  }));

fs.copyFile(src, nulldesturl)
  .then(common.mustNotCall())
  .catch(common.expectsError({
    code: 'ERR_INVALID_ARG_TYPE',
    type: Error,
    message: 'The "path" argument must be of type string without null bytes. ' +
             'Received type string'
  }));

fs.copyFile(invalidsrcurl, dest)
  .then(common.mustNotCall())
  .catch(common.expectsError({
    code: 'ERR_INVALID_URL_SCHEME',
    type: TypeError,
    message: 'The URL must be of scheme file'
  }));

fs.copyFile(src, invaliddesturl)
  .then(common.mustNotCall())
  .catch(common.expectsError({
    code: 'ERR_INVALID_URL_SCHEME',
    type: TypeError,
    message: 'The URL must be of scheme file'
  }));

[-1, {}, [], Infinity, true, null, undefined].forEach((i) => {
  fs.copyFile(i, dest)
    .then(common.mustNotCall())
    .catch(common.expectsError({
      code: 'ERR_INVALID_ARG_TYPE',
      type: TypeError,
      message: 'The "src" argument must be one of type string, Buffer, or URL'
    }));

  fs.copyFile(src, i)
    .then(common.mustNotCall())
    .catch(common.expectsError({
      code: 'ERR_INVALID_ARG_TYPE',
      type: TypeError,
      message: 'The "dest" argument must be one of type string, Buffer, or URL'
    }));
});
