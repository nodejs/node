'use strict';

require('../common');
const fixtures = require('../common/fixtures');
const fs = require('fs');
const assert = require('assert');

const tmpdir = require('../common/tmpdir');
tmpdir.refresh();

const streamOpts = ['open', 'close'];
const writeStreamOptions = [...streamOpts, 'write'];
const readStreamOptions = [...streamOpts, 'read'];
const originalFs = { fs };

{
  const file = tmpdir.resolve('write-end-test0.txt');

  writeStreamOptions.forEach((fn) => {
    const overrideFs = Object.assign({}, originalFs.fs, { [fn]: null });
    if (fn === 'write') overrideFs.writev = null;

    const opts = {
      fs: overrideFs
    };
    assert.throws(
      () => fs.createWriteStream(file, opts), {
        code: 'ERR_INVALID_ARG_TYPE',
        name: 'TypeError',
        message: `The "options.fs.${fn}" property must be of type function. ` +
        'Received null'
      },
      `createWriteStream options.fs.${fn} should throw if isn't a function`
    );
  });
}

{
  const file = tmpdir.resolve('write-end-test0.txt');
  const overrideFs = Object.assign({}, originalFs.fs, { writev: 'not a fn' });
  const opts = {
    fs: overrideFs
  };
  assert.throws(
    () => fs.createWriteStream(file, opts), {
      code: 'ERR_INVALID_ARG_TYPE',
      name: 'TypeError',
      message: 'The "options.fs.writev" property must be of type function. ' +
        'Received type string (\'not a fn\')'
    },
    'createWriteStream options.fs.writev should throw if isn\'t a function'
  );
}

{
  const file = fixtures.path('x.txt');
  readStreamOptions.forEach((fn) => {
    const overrideFs = Object.assign({}, originalFs.fs, { [fn]: null });
    const opts = {
      fs: overrideFs
    };
    assert.throws(
      () => fs.createReadStream(file, opts), {
        code: 'ERR_INVALID_ARG_TYPE',
        name: 'TypeError',
        message: `The "options.fs.${fn}" property must be of type function. ` +
        'Received null'
      },
      `createReadStream options.fs.${fn} should throw if isn't a function`
    );
  });
}
