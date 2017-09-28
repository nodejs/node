'use strict';

const common = require('../common');
const {
  async: fs,
  Stats
} = require('fs');
const { Buffer } = require('buffer');
const { URL } = require('url');
const assert = require('assert');

common.crashOnUnhandledRejection();

common.refreshTmpDir();

async function test(method, input, open = false) {
  if (open) {
    input = await fs.open(input, 'r');
  }
  try {
    const stats = await method(input);
    assert(stats instanceof Stats);
  } finally {
    if (open)
      await fs.close(input);
  }
}

test(fs.fstat, __filename, true);

[-1, {}, [], Infinity, null, undefined, 'hello'].forEach((i) => {
  fs.fstat(i)
    .then(common.mustNotCall())
    .catch(common.expectsError({
      code: 'ERR_INVALID_ARG_TYPE',
      type: TypeError,
      message: 'The "fd" argument must be of type unsigned integer'
    }));
});

test(fs.stat, __filename)
  .then(common.mustCall())
  .catch(common.mustNotCall());

test(fs.stat, Buffer.from(__filename))
  .then(common.mustCall())
  .catch(common.mustNotCall());

test(fs.stat, new URL(`file://${__filename}`))
  .then(common.mustCall())
  .catch(common.mustNotCall());

test(fs.stat, `${__filename}-NO`)
  .then(common.mustNotCall())
  .catch(common.expectsError({
    code: 'ENOENT',
    type: Error,
    message: `ENOENT: no such file or directory, stat '${__filename}-NO'`
  }));

test(fs.stat, `${__filename}-\u0000`)
  .then(common.mustNotCall())
  .catch(common.expectsError({
    code: 'ERR_INVALID_ARG_TYPE',
    type: Error,
    message: 'The "path" argument must be of type string without null bytes. ' +
             'Received type string'
  }));

test(fs.stat, new URL(`file://${__filename}-%00`))
  .then(common.mustNotCall())
  .catch(common.expectsError({
    code: 'ERR_INVALID_ARG_TYPE',
    type: Error,
    message: 'The "path" argument must be of type string without null bytes. ' +
             'Received type string'
  }));

[-1, {}, [], Infinity, undefined, null, true].forEach((i) => {
  test(fs.stat, i)
    .then(common.mustNotCall())
    .catch(common.expectsError({
      code: 'ERR_INVALID_ARG_TYPE',
      type: TypeError,
      message: 'The "path" argument must be one of type string, Buffer, or URL'
    }));
});
