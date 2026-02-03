'use strict';

const common = require('../common');

// Test for FileHandle class accessibility and isFileHandle method
// This validates the implementation of GitHub issue #61637

const fs = require('fs');
const fsPromises = require('fs/promises');
const path = require('path');
const assert = require('assert');
const tmpdir = require('../common/tmpdir');
const tmpDir = tmpdir.path;

tmpdir.refresh();

async function testFileHandleAccessibility() {
  // Test 1: FileHandle should be accessible from fs.FileHandle
  assert(fs.FileHandle !== undefined, 'fs.FileHandle should be defined');
  assert(typeof fs.FileHandle === 'function', 'fs.FileHandle should be a function/class');

  // Test 2: FileHandle should be accessible from fs.promises.FileHandle
  assert(fsPromises.FileHandle !== undefined, 'fs.promises.FileHandle should be defined');
  assert(typeof fsPromises.FileHandle === 'function', 'fs.promises.FileHandle should be a function/class');

  // Test 3: Both references should point to the same class
  assert.strictEqual(fs.FileHandle, fsPromises.FileHandle, 'fs.FileHandle and fs.promises.FileHandle should be the same');
}

async function testIsFileHandleMethod() {
  // Test 4: FileHandle.isFileHandle should exist
  assert(typeof fs.FileHandle.isFileHandle === 'function', 'FileHandle.isFileHandle should be a function');

  // Test 5: Test isFileHandle with actual FileHandle instance
  const testFilePath = path.join(tmpDir, 'test_filehandle.txt');
  await fsPromises.writeFile(testFilePath, 'test content');

  const fileHandle = await fsPromises.open(testFilePath, 'r');
  try {
    assert(fs.FileHandle.isFileHandle(fileHandle) === true, 'isFileHandle should return true for FileHandle instance');
  } finally {
    await fileHandle.close();
  }

  // Test 6: Test instanceof check
  const fileHandle2 = await fsPromises.open(testFilePath, 'r');
  try {
    assert(fileHandle2 instanceof fs.FileHandle, 'fileHandle should be instance of FileHandle');
    assert(fileHandle2 instanceof fsPromises.FileHandle, 'fileHandle should be instance of fs.promises.FileHandle');
  } finally {
    await fileHandle2.close();
  }

  // Test 7: Test isFileHandle with non-FileHandle objects
  assert(fs.FileHandle.isFileHandle({}) === false, 'isFileHandle should return false for plain object');
  assert(fs.FileHandle.isFileHandle(null) === false, 'isFileHandle should return false for null');
  assert(fs.FileHandle.isFileHandle(undefined) === false, 'isFileHandle should return false for undefined');
  assert(fs.FileHandle.isFileHandle(5) === false, 'isFileHandle should return false for number (fd)');
  assert(fs.FileHandle.isFileHandle('path/to/file') === false, 'isFileHandle should return false for string');
  assert(fs.FileHandle.isFileHandle(Buffer.from('test')) === false, 'isFileHandle should return false for Buffer');

  // Clean up
  await fsPromises.unlink(testFilePath);
}

async function testFileHandleUsage() {
  // Test 8: Demonstrate practical usage - function accepting multiple input types
  const testFilePath = path.join(tmpDir, 'test_usage.txt');
  await fsPromises.writeFile(testFilePath, 'Hello FileHandle!');

  async function flexibleReadFile(input) {
    if (fs.FileHandle.isFileHandle(input)) {
      // Input is already a FileHandle
      return await input.readFile({ encoding: 'utf8' });
    } else if (typeof input === 'string') {
      // Input is a file path
      const fh = await fsPromises.open(input, 'r');
      try {
        return await fh.readFile({ encoding: 'utf8' });
      } finally {
        await fh.close();
      }
    } else {
      throw new TypeError('Input must be a file path or FileHandle');
    }
  }

  // Test with file path
  const content1 = await flexibleReadFile(testFilePath);
  assert.strictEqual(content1, 'Hello FileHandle!');

  // Test with FileHandle
  const fh = await fsPromises.open(testFilePath, 'r');
  try {
    const content2 = await flexibleReadFile(fh);
    assert.strictEqual(content2, 'Hello FileHandle!');
  } finally {
    await fh.close();
  }

  // Clean up
  await fsPromises.unlink(testFilePath);
}

(async () => {
  await testFileHandleAccessibility();
  await testIsFileHandleMethod();
  await testFileHandleUsage();
})().then(common.mustCall());