'use strict';
const common = require('../common');
const assert = require('assert');
const fs = require('fs');
const path = require('path');
const tmpdir = require('../common/tmpdir');

// Test the coroutine-based fs bindings (proof of concept).
// These bindings use C++20 coroutines to implement async fs operations
// as sequential code with co_await, replacing the callback/ReqWrap pattern.

const binding = process.binding('fs');

tmpdir.refresh();

// Test 1: coroAccess — check that an existing file is accessible
{
  const testFile = path.join(tmpdir.path, 'coro-access-test.txt');
  fs.writeFileSync(testFile, 'hello');

  binding.coroAccess(testFile, fs.constants.R_OK)
    .then(common.mustCall((result) => {
      assert.strictEqual(result, undefined);
    }));
}

// Test 2: coroAccess — check that a nonexistent file rejects
{
  const noSuchFile = path.join(tmpdir.path, 'nonexistent-coro-file.txt');

  binding.coroAccess(noSuchFile, fs.constants.R_OK)
    .then(common.mustNotCall('should have rejected'))
    .catch(common.mustCall((err) => {
      assert.strictEqual(err.code, 'ENOENT');
      assert.strictEqual(err.syscall, 'access');
    }));
}

// Test 3: coroReadFileBytes — read a file and get a Buffer back
{
  const testFile = path.join(tmpdir.path, 'coro-read-test.txt');
  const testData = 'Hello from coroutine readFile!';
  fs.writeFileSync(testFile, testData);

  binding.coroReadFileBytes(testFile)
    .then(common.mustCall((buf) => {
      assert(Buffer.isBuffer(buf), 'result should be a Buffer');
      assert.strictEqual(buf.toString(), testData);
    }));
}

// Test 4: coroReadFileBytes — read a larger file
{
  const testFile = path.join(tmpdir.path, 'coro-read-large-test.txt');
  // Create a file larger than a typical buffer size
  const testData = 'x'.repeat(100_000);
  fs.writeFileSync(testFile, testData);

  binding.coroReadFileBytes(testFile)
    .then(common.mustCall((buf) => {
      assert(Buffer.isBuffer(buf));
      assert.strictEqual(buf.length, 100_000);
      assert.strictEqual(buf.toString(), testData);
    }));
}

// Test 5: coroReadFileBytes — nonexistent file rejects
{
  const noSuchFile = path.join(tmpdir.path, 'nonexistent-coro-read.txt');

  binding.coroReadFileBytes(noSuchFile)
    .then(common.mustNotCall('should have rejected'))
    .catch(common.mustCall((err) => {
      assert.strictEqual(err.code, 'ENOENT');
      assert.strictEqual(err.syscall, 'open');
    }));
}

// Test 6: coroReadFileBytes — read an empty file
{
  const testFile = path.join(tmpdir.path, 'coro-read-empty-test.txt');
  fs.writeFileSync(testFile, '');

  binding.coroReadFileBytes(testFile)
    .then(common.mustCall((buf) => {
      assert(Buffer.isBuffer(buf));
      assert.strictEqual(buf.length, 0);
    }));
}

// Test 7: coroReadFileBytes — read binary data
{
  const testFile = path.join(tmpdir.path, 'coro-read-binary-test.bin');
  const binaryData = Buffer.from([0x00, 0x01, 0x02, 0xFF, 0xFE, 0xFD]);
  fs.writeFileSync(testFile, binaryData);

  binding.coroReadFileBytes(testFile)
    .then(common.mustCall((buf) => {
      assert(Buffer.isBuffer(buf));
      assert.deepStrictEqual(buf, binaryData);
    }));
}
