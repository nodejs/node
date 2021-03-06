'use strict';

const common = require('../common');

// The following tests validate base functionality for the fs.promises
// FileHandle.write method.

const fs = require('fs');
const { open } = fs.promises;
const path = require('path');
const tmpdir = require('../common/tmpdir');
const assert = require('assert');
const tmpDir = tmpdir.path;

tmpdir.refresh();

async function validateWrite() {
  const filePathForHandle = path.resolve(tmpDir, 'tmp-write.txt');
  const fileHandle = await open(filePathForHandle, 'w+');
  const buffer = Buffer.from('Hello world'.repeat(100), 'utf8');

  await fileHandle.write(buffer, 0, buffer.length);
  const readFileData = fs.readFileSync(filePathForHandle);
  assert.deepStrictEqual(buffer, readFileData);

  await fileHandle.close();
}

async function validateEmptyWrite() {
  const filePathForHandle = path.resolve(tmpDir, 'tmp-empty-write.txt');
  const fileHandle = await open(filePathForHandle, 'w+');
  const buffer = Buffer.from(''); // empty buffer

  await fileHandle.write(buffer, 0, buffer.length);
  const readFileData = fs.readFileSync(filePathForHandle);
  assert.deepStrictEqual(buffer, readFileData);

  await fileHandle.close();
}

async function validateNonUint8ArrayWrite() {
  const filePathForHandle = path.resolve(tmpDir, 'tmp-data-write.txt');
  const fileHandle = await open(filePathForHandle, 'w+');
  const buffer = Buffer.from('Hello world', 'utf8').toString('base64');

  await fileHandle.write(buffer, 0, buffer.length);
  const readFileData = fs.readFileSync(filePathForHandle);
  assert.deepStrictEqual(Buffer.from(buffer, 'utf8'), readFileData);

  await fileHandle.close();
}

async function validateNonStringValuesWrite() {
  const filePathForHandle = path.resolve(tmpDir, 'tmp-non-string-write.txt');
  const fileHandle = await open(filePathForHandle, 'w+');
  const nonStringValues = [123, {}, new Map()];
  for (const nonStringValue of nonStringValues) {
    await assert.rejects(
      fileHandle.write(nonStringValue),
      { message: /"buffer"/, code: 'ERR_INVALID_ARG_TYPE' }
    );
  }

  await fileHandle.close();
}

Promise.all([
  validateWrite(),
  validateEmptyWrite(),
  validateNonUint8ArrayWrite(),
  validateNonStringValuesWrite()
]).then(common.mustCall());
