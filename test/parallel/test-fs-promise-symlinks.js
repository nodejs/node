'use strict';

const common = require('../common');
if (!common.canCreateSymLink())
  common.skip('insufficient privileges');
const {
  async: fs,
  closeSync,
  openSync
} = require('fs');
const assert = require('assert');
const { Buffer } = require('buffer');
const { URL } = require('url');
const path = require('path');

common.crashOnUnhandledRejection();

common.refreshTmpDir();

const src = path.join(common.tmpDir, 'a.js');
const dest = path.join(common.tmpDir, 'b.js');
closeSync(openSync(src, 'w'));

const nullsrcurl = new URL(`file://${src}-%00NO`);
const invalidsrcurl = new URL(`http://${src}`);

const nulldesturl = new URL(`file://${dest}-%00NO`);
const invaliddesturl = new URL(`http://${dest}`);

async function test(s, d) {
  await fs.symlink(s, d);
  assert.strictEqual(src, await fs.readlink(d));
  assert.strictEqual(src, await fs.realpath(d));
}

test(src, dest)
  .then(common.mustCall())
  .catch(common.mustNotCall());

test(Buffer.from(src), `${dest}-1`)
  .then(common.mustCall())
  .catch(common.mustNotCall());

test(src, Buffer.from(`${dest}-2`))
  .then(common.mustCall())
  .catch(common.mustNotCall());

test(new URL(`file://${src}`), `${dest}-3`)
  .then(common.mustCall())
  .catch(common.mustNotCall());

test(src, new URL(`file://${dest}-4`))
  .then(common.mustCall())
  .catch(common.mustNotCall());

test(nullsrcurl, dest)
  .then(common.mustNotCall())
  .catch(common.expectsError({
    code: 'ERR_INVALID_ARG_TYPE',
    type: Error,
    message: 'The "path" argument must be of type string without null bytes. ' +
             'Received type string'
  }));

test(invalidsrcurl, dest)
  .then(common.mustNotCall())
  .catch(common.expectsError({
    code: 'ERR_INVALID_URL_SCHEME',
    type: TypeError,
    message: 'The URL must be of scheme file'
  }));

test(src, nulldesturl)
  .then(common.mustNotCall())
  .catch(common.expectsError({
    code: 'ERR_INVALID_ARG_TYPE',
    type: Error,
    message: 'The "path" argument must be of type string without null bytes. ' +
             'Received type string'
  }));

test(src, invaliddesturl)
  .then(common.mustNotCall())
  .catch(common.expectsError({
    code: 'ERR_INVALID_URL_SCHEME',
    type: TypeError,
    message: 'The URL must be of scheme file'
  }));
