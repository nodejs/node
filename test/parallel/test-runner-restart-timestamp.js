// Flags: --expose-internals
'use strict';

const common = require('../common');
const assert = require('assert');
const { TestsStream } = require('internal/test_runner/tests_stream');

// ✅ Instantiate the TestsStream reporter directly
const reporter = new TestsStream();

// ✅ Patch to monitor the console output
let consoleOutput = '';
const originalWrite = process.stdout.write;
process.stdout.write = (chunk, ...args) => {
  consoleOutput += chunk;
  return originalWrite.call(process.stdout, chunk, ...args);
};

// ✅ Wrap timeStamp with mustCall (this ensures it is invoked)
common.mustCall(() => {
  reporter.timeStamp('test:restarted', { file: 'dummy.js' });
})();

// ✅ Restore console
process.stdout.write = originalWrite;

// ✅ Assert output contains the timestamp message
assert.ok(
  consoleOutput.includes('INFO: Test restarted'),
  'Expected timestamp message in console output'
);
