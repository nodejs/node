'use strict';
const common = require('../common');
if (common.isIBMi)
  common.skip('On IBMi, the rss memory always returns zero');

// This test verifies that readFileSync does not leak memory when reading
// UTF8 files. See: https://github.com/nodejs/node/issues/57800 for details.

const assert = require('node:assert');
const fs = require('node:fs');
const util = require('node:util');
const tmpdir = require('../common/tmpdir');

// The memory leak being tested here was from a buffer with the absolute URI to
// a file. For each file read, 2-4 bytes were (usually) leaked per character in
// the URI. The length of the file path can be used to estimate the approximate
// amount of memory that will be leaked if this issue is reintroduced. A longer
// total path length will make the issue easier to test for. Some operating
// systems like Windows have shorter default path length limits. If the path
// is approaching that limit, the length of the path should be long enough to
// effectively test for this memory leak.
tmpdir.refresh();
let testFile = tmpdir.resolve(
  'a-file-with-a-longer-than-usual-file-name-for-testing-a-memory-leak-related-to-path-length.txt',
);
if (testFile.length > process.env.MAX_PATH) {
  testFile = tmpdir.resolve('reasonable-length.txt');
}

// The buffer being checked is WCHAR buffer. The size is going to be at least 2
// bytes per character but can be more. Windows: 2; Mac: 2; Linux: 4 (usually);
const iterations = 100_000;
const minExpectedMemoryLeak = (testFile.length * 2) * iterations;

// This memory leak was exclusive to UTF8 encoded files.
// Create our test file. The contents of the file don't matter.
const options = { encoding: 'utf8' };
fs.writeFileSync(testFile, '', options);

// Doing an initial big batch of file reads gives a more stable baseline memory
// usage. Doing the same total iterations as the actual test isn't necessary.
for (let i = 0; i < 100; i++) {
  fs.readFileSync(testFile, options);
}
const startMemory = process.memoryUsage();
for (let i = 0; i < iterations; i++) {
  fs.readFileSync(testFile, options);
}
const endMemory = process.memoryUsage();

// Use 90% of the expected memory leak as the threshold just to be safe.
const memoryDifference = endMemory.rss - startMemory.rss;
assert.ok(memoryDifference < (minExpectedMemoryLeak * 0.9),
          `Unexpected memory overhead: ${util.inspect([startMemory, endMemory])}`);
