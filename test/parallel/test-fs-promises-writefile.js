'use strict';

const common = require('../common');
const fs = require('fs');
const path = require('path');
const tmpdir = require('../common/tmpdir');
const assert = require('assert');
const tmpDir = tmpdir.path;

tmpdir.refresh();

common.crashOnUnhandledRejection();

const dest = path.resolve(tmpDir, 'tmp.txt');
const buffer = Buffer.from('abc'.repeat(1000));
const buffer2 = Buffer.from('xyz'.repeat(1000));

async function doWrite() {
  await fs.promises.writeFile(dest, buffer);
  const data = fs.readFileSync(dest);
  assert.deepStrictEqual(data, buffer);
}

async function doAppend() {
  await fs.promises.appendFile(dest, buffer2);
  const data = fs.readFileSync(dest);
  const buf = Buffer.concat([buffer, buffer2]);
  assert.deepStrictEqual(buf, data);
}

async function doRead() {
  const data = await fs.promises.readFile(dest);
  const buf = fs.readFileSync(dest);
  assert.deepStrictEqual(buf, data);
}

doWrite()
  .then(doAppend)
  .then(doRead)
  .then(common.mustCall());
