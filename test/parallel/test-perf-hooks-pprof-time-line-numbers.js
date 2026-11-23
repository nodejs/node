'use strict';

// Checks that the time profiler can aggregate samples at the line level.

require('../common');
const assert = require('node:assert');
const fs = require('node:fs');
const {
  decodeGzippedProfile,
  pprofTestBusyLoop,
} = require('../common/pprof');
const { time, encodeSync } = require('node:perf_hooks/pprof');

/**
 * Returns the lines of a file.
 * @param {string} file The path of the file.
 * @returns {string[]} The lines of the file, without their line terminators.
 */
function readLines(file) {
  return fs.readFileSync(file, 'utf8').split(/\r?\n/);
}

// The first and last lines of pprofTestBusyLoop, from the source of its
// module, and the line of its call, from the source of this test.
const loopFile = require.resolve('../common/pprof');
const loopLines = readLines(loopFile);
const functionLine =
  loopLines.indexOf('function pprofTestBusyLoop(cpuMicros) {') + 1;
const functionEndLine =
  functionLine + loopLines.slice(functionLine).indexOf('}') + 1;
const callLine = readLines(__filename).indexOf('pprofTestBusyLoop(300_000);') + 1;
assert.ok(functionLine > 0);
assert.ok(functionEndLine > functionLine);
assert.ok(callLine > 0);

time.start({ lineNumbers: true });
pprofTestBusyLoop(300_000);
const { samples } = decodeGzippedProfile(encodeSync(time.stop()));

const leaves = samples
  .map(({ stack }) => stack[0])
  .filter(({ name }) => name === 'pprofTestBusyLoop');
assert.ok(leaves.length > 0);
// Samples in the body of the function are at lines after its first one.
for (const { line, file } of leaves) {
  assert.strictEqual(file, loopFile);
  assert.ok(line > functionLine, `${line}`);
  assert.ok(line <= functionEndLine, `${line}`);
}

// In caller line number mode, the line of a non-leaf frame is the line of the
// call it makes, e.g. `callLine` for the frame that calls pprofTestBusyLoop.
const callers = samples
  .filter(({ stack }) => stack[0].name === 'pprofTestBusyLoop')
  .map(({ stack }) => stack[1]);
assert.ok(callers.some(({ line }) => line === callLine), `${callLine}`);
