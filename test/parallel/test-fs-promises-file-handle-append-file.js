'use strict';

const common = require('../common');

// The following tests validate base functionality for the fs/promises
// FileHandle.appendFile method.

const fs = require('fs');
const { open } = require('fs/promises');
const path = require('path');
const tmpdir = require('../common/tmpdir');
const assert = require('assert');
const tmpDir = tmpdir.path;

async function validateAppendBuffer({ filePath,
                                      fileHandle,
                                      bufferToAppend }) {
  await fileHandle.appendFile(bufferToAppend);
  const appendedFileData = fs.readFileSync(filePath);
  assert.deepStrictEqual(appendedFileData, bufferToAppend);
}

async function validateAppendString({ filePath,
                                      fileHandle,
                                      stringToAppend,
                                      bufferToAppend }) {
  await fileHandle.appendFile(stringToAppend);
  const stringAsBuffer = Buffer.from(stringToAppend, 'utf8');
  const appendedFileData = fs.readFileSync(filePath);
  const combinedBuffer = Buffer.concat([bufferToAppend, stringAsBuffer]);
  assert.deepStrictEqual(appendedFileData, combinedBuffer);
}

async function executeTests() {
  tmpdir.refresh();
  common.crashOnUnhandledRejection();

  const filePath = path.resolve(tmpDir, 'tmp-append-file.txt');
  const appendFileDetails = {
    filePath,
    fileHandle: await open(filePath, 'a'),
    bufferToAppend: Buffer.from('a&Dp'.repeat(100), 'utf8'),
    stringToAppend: 'x~yz'.repeat(100)
  };

  await validateAppendBuffer(appendFileDetails);
  await validateAppendString(appendFileDetails);
}

executeTests().then(common.mustCall());
