'use strict';

// This tests if the writeFile automatically adjusts the file offset even if
// the file is truncated.

const common = require('../common');
const fsPromises = require('fs').promises;
const path = require('path');
const assert = require('assert');
const tmpdir = require('../common/tmpdir');
const tmpDir = tmpdir.path;
const Data = 'FS Promises';

tmpdir.refresh();

(async function() {
  assert(!/A/.test(Data));
  const fileName = path.resolve(tmpDir, 'temp.txt');
  const fileHandle = await fsPromises.open(fileName, 'w');
  await fileHandle.writeFile('A'.repeat(Data.length * 2));
  await fileHandle.truncate();
  await fileHandle.writeFile(Data);
  await fileHandle.close();
  assert.deepStrictEqual(await fsPromises.readFile(fileName, 'UTF-8'), Data);
})().then(common.mustCall());

(async function() {
  // In this case there is no truncate call, but still the contents of the file
  // should be truncated before the new string is written.
  assert(!/A/.test(Data));
  const fileName = path.resolve(tmpDir, 'temp.txt');
  const fileHandle = await fsPromises.open(fileName, 'w');
  await fileHandle.writeFile('A'.repeat(Data.length * 2));
  await fileHandle.writeFile(Data);
  await fileHandle.close();
  assert.deepStrictEqual(await fsPromises.readFile(fileName, 'UTF-8'), Data);
})().then(common.mustCall());

(async function() {
  // This tests appendFile as well, as appendFile internally uses writeFile
  const fileName = path.resolve(tmpDir, 'temp-1.txt');
  const fileHandle = await fsPromises.open(fileName, 'w');
  await fileHandle.writeFile('A'.repeat(Data.length * 2));
  await fileHandle.truncate();
  await fileHandle.appendFile(Data);
  await fileHandle.close();
  assert.deepStrictEqual(await fsPromises.readFile(fileName, 'UTF-8'), Data);
})().then(common.mustCall());
