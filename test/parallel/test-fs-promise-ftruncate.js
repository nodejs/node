'use strict';

const common = require('../common');
const {
  async: fs,
  closeSync,
  openSync
} = require('fs');
const path = require('path');
const {
  Buffer
} = require('buffer');
const {
  URL
} = require('url');

common.crashOnUnhandledRejection();

common.refreshTmpDir();

const file = path.join(common.tmpDir, 'a.js');
closeSync(openSync(file, 'w'));

async function test(file, len) {
  const fd = await fs.open(file, 'w');
  try {
    await fs.ftruncate(fd, len);
  } finally {
    await fs.close(fd);
  }
}

async function test2(file, len) {
  await fs.truncate(file, len);
}

test(file, 0)
  .then(common.mustCall())
  .catch(common.mustNotCall());

test(file, 1)
  .then(common.mustCall())
  .catch(common.mustNotCall());

[-1, {}, [], '-1', Infinity].forEach((i) => {
  fs.ftruncate(i)
    .then(common.mustNotCall)
    .catch(common.expectsError({
      code: 'ERR_INVALID_ARG_TYPE',
      type: TypeError,
      message: 'The "fd" argument must be of type unsigned integer'
    }));

  test(file, i)
    .then(common.mustNotCall)
    .catch(common.expectsError({
      code: 'ERR_INVALID_ARG_TYPE',
      type: TypeError,
      message: 'The "len" argument must be of type unsigned integer'
    }));

  test2(file, i)
    .then(common.mustNotCall)
    .catch(common.expectsError({
      code: 'ERR_INVALID_ARG_TYPE',
      type: TypeError,
      message: 'The "len" argument must be of type unsigned integer'
    }));
});

test2(file, 0)
  .then(common.mustCall())
  .catch(common.mustNotCall());

test2(Buffer.from(file), 0)
  .then(common.mustCall())
  .catch(common.mustNotCall());

test2(new URL(`file://${file}`), 0)
  .then(common.mustCall())
  .catch(common.mustNotCall());
