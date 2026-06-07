'use strict';

// Test that fs.readFileSync and fs.writeFileSync accept all valid
// UTF-8 encoding names (case-insensitive) for the fast path.
// Refs: https://github.com/nodejs/node/issues/49888

const common = require('../common');
const fs = require('fs');
const path = require('path');
const assert = require('assert');
const tmpdir = require('../common/tmpdir');

tmpdir.refresh();

const testFile = path.join(tmpdir.path, 'test-utf8-encoding.txt');
const testContent = 'Hello, World! 你好，世界！';

// All valid UTF-8 encoding variants that should use the fast path
const utf8Variants = [
  'utf8',
  'utf-8',
  'UTF8',
  'UTF-8',
  'Utf8',
  'Utf-8',
  'uTf8',
  'uTf-8',
  'utF8',
  'utF-8',
  'UTf8',
  'UTf-8',
  'uTF8',
  'uTF-8',
];

// Test writeFileSync with all UTF-8 variants
for (const encoding of utf8Variants) {
  const testPath = path.join(tmpdir.path, `test-write-${encoding}.txt`);

  // Should not throw and should write the file correctly
  fs.writeFileSync(testPath, testContent, { encoding });

  // Verify the file was written correctly
  const content = fs.readFileSync(testPath, 'utf8');
  assert.strictEqual(content, testContent,
                     `writeFileSync should work with encoding "${encoding}"`);
}

// Test readFileSync with all UTF-8 variants
for (const encoding of utf8Variants) {
  const testPath = path.join(tmpdir.path, `test-read-${encoding}.txt`);

  // Create a test file
  fs.writeFileSync(testPath, testContent, 'utf8');

  // Should read the file correctly with any UTF-8 variant
  const content = fs.readFileSync(testPath, { encoding });
  assert.strictEqual(content, testContent,
                     `readFileSync should work with encoding "${encoding}"`);
}

// Test that non-UTF-8 encodings still work correctly
const otherEncodings = ['ascii', 'base64', 'hex', 'latin1'];
for (const encoding of otherEncodings) {
  const testPath = path.join(tmpdir.path, `test-${encoding}.txt`);

  // These should not use the UTF-8 fast path but should still work
  fs.writeFileSync(testPath, testContent, { encoding });
  const content = fs.readFileSync(testPath, { encoding });
  assert.strictEqual(content, testContent,
                     `Should work with encoding "${encoding}"`);
}

console.log('All tests passed!');
