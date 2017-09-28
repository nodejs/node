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

common.refreshTmpDir();

const src = path.join(common.tmpDir, 'a.js');
const dest = path.join(common.tmpDir, 'b.js');
closeSync(openSync(src, 'w'));

const goodsrcurl = new URL(`file://${src}`);
const badsrcurl = new URL(`file://${src}-NO`);
const nullsrcurl = new URL(`file://${src}-%00NO`);
const invalidsrcurl = new URL(`http://${src}`);

const gooddesturl = new URL(`file://${dest}-4`);
const nulldesturl = new URL(`file://${dest}-%00NO`);
const invaliddesturl = new URL(`http://${dest}`);

async function test(src, dest) {
  await fs.link(src, dest);
  await fs.unlink(dest);
}

test(src, `${dest}-1`)
  .then(common.mustCall())
  .catch(common.mustNotCall());

test(Buffer.from(src), `${dest}-2`)
  .then(common.mustCall())
  .catch(common.mustNotCall());

test(goodsrcurl, `${dest}-3`)
  .then(common.mustCall())
  .catch(common.mustNotCall());

test(src, gooddesturl)
  .then(common.mustCall())
  .catch(common.mustNotCall());

test(goodsrcurl, Buffer.from(`${dest}-5`))
  .then(common.mustCall())
  .catch(common.mustNotCall());

fs.link(badsrcurl, dest)
  .then(common.mustNotCall())
  .catch(common.expectsError({
    code: 'ENOENT',
    type: Error,
    message:
      `ENOENT: no such file or directory, link '${src}-NO' -> '${dest}'`
  }));

fs.link(nullsrcurl, dest)
  .then(common.mustNotCall())
  .catch(common.expectsError({
    code: 'ERR_INVALID_ARG_TYPE',
    type: Error,
    message: 'The "path" argument must be of type string without null bytes. ' +
             'Received type string'
  }));

fs.link(src, nulldesturl)
  .then(common.mustNotCall())
  .catch(common.expectsError({
    code: 'ERR_INVALID_ARG_TYPE',
    type: Error,
    message: 'The "path" argument must be of type string without null bytes. ' +
             'Received type string'
  }));

fs.link(invalidsrcurl, dest)
  .then(common.mustNotCall())
  .catch(common.expectsError({
    code: 'ERR_INVALID_URL_SCHEME',
    type: TypeError,
    message: 'The URL must be of scheme file'
  }));

fs.link(src, invaliddesturl)
  .then(common.mustNotCall())
  .catch(common.expectsError({
    code: 'ERR_INVALID_URL_SCHEME',
    type: TypeError,
    message: 'The URL must be of scheme file'
  }));

[-1, {}, [], Infinity, true, null, undefined].forEach((i) => {
  fs.unlink(i)
    .then(common.mustNotCall())
    .catch(common.expectsError({
      code: 'ERR_INVALID_ARG_TYPE',
      type: TypeError,
      message: 'The "path" argument must be one of type string, Buffer, or URL'
    }));

  fs.link(i, dest)
    .then(common.mustNotCall())
    .catch(common.expectsError({
      code: 'ERR_INVALID_ARG_TYPE',
      type: TypeError,
      message: 'The "src" argument must be one of type string, Buffer, or URL'
    }));

  fs.link(src, i)
    .then(common.mustNotCall())
    .catch(common.expectsError({
      code: 'ERR_INVALID_ARG_TYPE',
      type: TypeError,
      message: 'The "dest" argument must be one of type string, Buffer, or URL'
    }));
});
