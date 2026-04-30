'use strict';

// This test ensures that fs.readFileSync and fs.writeFileSync
// accept all valid UTF8 encoding variants (utf8, utf-8, UTF8, UTF-8).
// Refs: https://github.com/nodejs/node/issues/49888

const common = require('../common');
const tmpdir = require('../../test/common/tmpdir');
const assert = require('assert');
const fs = require('fs');
const path = require('path');

tmpdir.refresh();

const testContent = 'Hello, World! 你好，世界！';
const encodings = ['utf8', 'utf-8', 'UTF8', 'UTF-8'];

// Test writeFileSync and readFileSync with different UTF8 variants
for (const encoding of encodings) {
  const testFile = tmpdir.resolve(`test_utf8_fast_path_${encoding}.txt`);

  // Test writeFileSync
  fs.writeFileSync(testFile, testContent, { encoding });

  // Test readFileSync
  const result = fs.readFileSync(testFile, { encoding });
  assert.strictEqual(result, testContent,
    `readFileSync should return correct content for encoding "${encoding}"`);
}

// Test with file descriptor
for (const encoding of encodings) {
  const testFile = tmpdir.resolve(`test_utf8_fast_path_fd_${encoding}.txt`);

  // Write with fd
  const fdWrite = fs.openSync(testFile, 'w');
  fs.writeFileSync(fdWrite, testContent, { encoding });
  fs.closeSync(fdWrite);

  // Read with fd
  const fdRead = fs.openSync(testFile, 'r');
  const result = fs.readFileSync(fdRead, { encoding });
  fs.closeSync(fdRead);

  assert.strictEqual(result, testContent,
    `readFileSync with fd should return correct content for encoding "${encoding}"`);
}

console.log('All UTF8 fast path casing tests passed!');
