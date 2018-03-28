'use strict';

const common = require('../common');

const assert = require('assert');
const path = require('path');
const { writeFile, readFile } = require('fs/promises');
const tmpdir = require('../common/tmpdir');
tmpdir.refresh();

const fn = path.join(tmpdir.path, 'large-file');

common.crashOnUnhandledRejection();

// Creating large buffer with random content
const buffer = Buffer.from(
  Array.apply(null, { length: 16834 * 2 })
    .map(Math.random)
    .map((number) => (number * (1 << 8)))
);

// Writing buffer to a file then try to read it
writeFile(fn, buffer)
  .then(() => readFile(fn))
  .then((readBuffer) => {
    assert.strictEqual(readBuffer.equals(buffer), true);
  })
  .then(common.mustCall());
