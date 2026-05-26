'use strict';

const common = require('../common');
const tmpdir = require('../common/tmpdir');
const assert = require('assert');
const fs = require('fs');
const path = require('path');

// This test ensures that the utf8 fast paths for readFileSync and writeFileSync
// accept all valid utf8 encoding names, including uppercase variants.
// Refs: https://github.com/nodejs/node/issues/49888

tmpdir.refresh();

const encodings = ['utf8', 'utf-8', 'UTF8', 'UTF-8'];
const testData = 'Hello, World! \u4e2d\u6587\u6d4b\u8bd5 \ud83d\ude00';
const fixture = path.join(tmpdir.path, 'test-utf8-fast-encoding.txt');

// Test writeFileSync with all encoding variants
for (const encoding of encodings) {
  fs.writeFileSync(fixture, testData, encoding);
  const result = fs.readFileSync(fixture, 'utf8');
  assert.strictEqual(result, testData,
    `writeFileSync with encoding "${encoding}" should produce correct output`);
}

// Test readFileSync with all encoding variants
fs.writeFileSync(fixture, testData, 'utf8');
for (const encoding of encodings) {
  const result = fs.readFileSync(fixture, encoding);
  assert.strictEqual(result, testData,
    `readFileSync with encoding "${encoding}" should produce correct output`);
}

// Test readFileSync with encoding option object
for (const encoding of encodings) {
  const result = fs.readFileSync(fixture, { encoding });
  assert.strictEqual(result, testData,
    `readFileSync with options.encoding "${encoding}" should produce correct output`);
}

// Test writeFileSync with encoding option object
for (const encoding of encodings) {
  fs.writeFileSync(fixture, testData, { encoding });
  const result = fs.readFileSync(fixture, 'utf8');
  assert.strictEqual(result, testData,
    `writeFileSync with options.encoding "${encoding}" should produce correct output`);
}
