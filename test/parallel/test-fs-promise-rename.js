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

const nullurl = new URL(`file://${src}-%00NO`);
const invalidurl = new URL(`http://${src}`);

async function test(s, d) {
  closeSync(openSync(s, 'w'));
  await fs.rename(s, d);
}

test(src, dest)
  .then(common.mustCall())
  .catch(common.mustNotCall());

test(Buffer.from(`${src}-1`), `${dest}-1`)
  .then(common.mustCall())
  .catch(common.mustNotCall());

test(`${src}-2`, Buffer.from(`${dest}-2`))
  .then(common.mustCall())
  .catch(common.mustNotCall());

test(new URL(`file://${src}-3`), `${dest}-3`)
  .then(common.mustCall())
  .catch(common.mustNotCall());

test(`${src}-4`, new URL(`file://${dest}-1`))
  .then(common.mustCall())
  .catch(common.mustNotCall());

fs.rename(nullurl, dest)
  .then(common.mustNotCall())
  .catch(common.expectsError({
    code: 'ERR_INVALID_ARG_TYPE',
    type: Error,
    message: 'The "path" argument must be of type string without null bytes. ' +
             'Received type string'
  }));

fs.rename(src, nullurl)
  .then(common.mustNotCall())
  .catch(common.expectsError({
    code: 'ERR_INVALID_ARG_TYPE',
    type: Error,
    message: 'The "path" argument must be of type string without null bytes. ' +
             'Received type string'
  }));

fs.rename(invalidurl, dest)
  .then(common.mustNotCall())
  .catch(common.expectsError({
    code: 'ERR_INVALID_URL_SCHEME',
    type: TypeError,
    message: 'The URL must be of scheme file'
  }));

fs.rename(src, invalidurl)
  .then(common.mustNotCall())
  .catch(common.expectsError({
    code: 'ERR_INVALID_URL_SCHEME',
    type: TypeError,
    message: 'The URL must be of scheme file'
  }));
