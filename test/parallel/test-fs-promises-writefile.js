'use strict';

const common = require('../common');
const fs = require('fs');
const fsPromises = fs.promises;
const path = require('path');
const tmpdir = require('../common/tmpdir');
const assert = require('assert');
const tmpDir = tmpdir.path;

tmpdir.refresh();

const dest = path.resolve(tmpDir, 'tmp.txt');
const buffer = Buffer.from('abc'.repeat(1000));
const buffer2 = Buffer.from('xyz'.repeat(1000));

async function doWrite() {
  await fsPromises.writeFile(dest, buffer);
  const data = fs.readFileSync(dest);
  assert.deepStrictEqual(data, buffer);
}

async function doAppend() {
  await fsPromises.appendFile(dest, buffer2);
  const data = fs.readFileSync(dest);
  const buf = Buffer.concat([buffer, buffer2]);
  assert.deepStrictEqual(buf, data);
}

async function doRead() {
  const data = await fsPromises.readFile(dest);
  const buf = fs.readFileSync(dest);
  assert.deepStrictEqual(buf, data);
}

async function doReadWithEncoding() {
  const data = await fsPromises.readFile(dest, 'utf-8');
  const syncData = fs.readFileSync(dest, 'utf-8');
  assert.strictEqual(typeof data, 'string');
  assert.deepStrictEqual(data, syncData);
}

doWrite()
  .then(doAppend)
  .then(doRead)
  .then(doReadWithEncoding)
  .then(common.mustCall());
