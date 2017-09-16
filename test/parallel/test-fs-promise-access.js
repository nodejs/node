'use strict';

const common = require('../common');
const { async: fs, F_OK } = require('fs');
const { Buffer } = require('buffer');
const { URL } = require('url');

const goodurl = new URL(`file://${__filename}`);
const badurl = new URL(`file://${__filename}-NO`);
const nullurl = new URL(`file://${__filename}-%00NO`);
const invalidurl = new URL(`http://${__filename}`);

common.crashOnUnhandledRejection();

// Access this file, known to exist. Must not fail.
fs.access(__filename)
  .then(common.mustCall())
  .catch(common.mustNotCall());

fs.access(goodurl)
  .then(common.mustCall())
  .catch(common.mustNotCall());

fs.access(__filename, F_OK)
  .then(common.mustCall())
  .catch(common.mustNotCall());

fs.access(goodurl, F_OK)
  .then(common.mustCall())
  .catch(common.mustNotCall());

fs.access(__dirname)
  .then(common.mustCall())
  .catch(common.mustNotCall());

fs.access(__dirname, F_OK)
  .then(common.mustCall())
  .catch(common.mustNotCall());

fs.access(`${__filename}-NO`)
  .then(common.mustNotCall())
  .catch(common.expectsError({
    code: 'ENOENT',
    type: Error,
    message: `ENOENT: no such file or directory, access '${__filename}-NO'`
  }));

fs.access(badurl)
  .then(common.mustNotCall())
  .catch(common.expectsError({
    code: 'ENOENT',
    type: Error,
    message: `ENOENT: no such file or directory, access '${__filename}-NO'`
  }));

fs.access(nullurl)
  .then(common.mustNotCall())
  .catch(common.expectsError({
    code: 'ERR_INVALID_ARG_TYPE',
    type: Error,
    message: 'The "path" argument must be of type string without null bytes. ' +
             'Received type string'
  }));

fs.access(invalidurl)
  .then(common.mustNotCall())
  .catch(common.expectsError({
    code: 'ERR_INVALID_URL_SCHEME',
    type: TypeError,
    message: 'The URL must be of scheme file'
  }));

fs.access(Buffer.from(__filename))
  .then(common.mustCall())
  .catch(common.mustNotCall());

fs.access(Buffer.from(`${__filename}-NO`))
  .then(common.mustNotCall())
  .catch(common.expectsError({
    code: 'ENOENT',
    type: Error,
    message: `ENOENT: no such file or directory, access '${__filename}-NO'`
  }));

[1, {}, [], Infinity, null, undefined, true].forEach((i) => {
  fs.access(i)
    .then(common.mustNotCall())
    .catch(common.expectsError({
      code: 'ERR_INVALID_ARG_TYPE',
      type: TypeError,
      message: 'The "path" argument must be one of type string, Buffer, or URL'
    }));
});

[{}, [], Infinity, null, true].forEach((i) => {
  fs.access(__filename, i)
    .then(common.mustNotCall())
    .catch(common.expectsError({
      code: 'ERR_INVALID_ARG_TYPE',
      type: TypeError,
      message: 'The "mode" argument must be of type integer'
    }));
});
