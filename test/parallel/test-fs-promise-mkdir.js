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

const dir = path.join(common.tmpDir, 'foo');

// mkdir
const goodurl = new URL(`file://${dir}-2`);
const nullurl = new URL(`file://${dir}-%00NO`);
const invalidurl = new URL(`http://${dir}`);

async function test(dir) {
  await fs.mkdir(dir);
  await fs.rmdir(dir);
}

test(dir)
  .then(common.mustCall())
  .catch(common.mustNotCall());

test(Buffer.from(`${dir}-1`))
  .then(common.mustCall())
  .catch(common.mustNotCall());

test(goodurl)
  .then(common.mustCall())
  .catch(common.mustNotCall());

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

[1, {}, [], Infinity, null, undefined, true].forEach((i) => {
  test(i)
    .then(common.mustNotCall())
    .catch(common.expectsError({
      code: 'ERR_INVALID_ARG_TYPE',
      type: TypeError,
      message: 'The "path" argument must be one of type string, Buffer, or URL'
    }));
});
