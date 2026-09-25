'use strict';
require('../common');
const tmpdir = require('../common/tmpdir');
const assert = require('node:assert');
const { spawnSync } = require('node:child_process');
const { writeFileSync, mkdirSync, readFileSync } = require('node:fs');
const { join } = require('node:path');

// Regression test for https://github.com/nodejs/node/issues/64833.
//
// When --test-force-exit is combined with process isolation and concurrency,
// each test file runs in a child process that streams its results back to the
// parent over a pipe. Writes to a pipe are asynchronous, so a forced
// process.exit() could truncate the still-flushing report stream, silently
// dropping test verdicts while the process still exited with code 0.
//
// Generate several test files, each of which keeps itself alive with a ref'd
// handle (so --test-force-exit is required to finish), then assert that every
// single verdict survives the forced exit.

const FILES = 12;
const TESTS_PER_FILE = 1000;
const EXPECTED = FILES * TESTS_PER_FILE;

tmpdir.refresh();
const testsDir = tmpdir.resolve('tests');
mkdirSync(testsDir);

for (let f = 0; f < FILES; f++) {
  let body = "'use strict';\n";
  body += "const { test } = require('node:test');\n";
  // A ref'd handle keeps the child alive so it never exits on its own; only
  // --test-force-exit ends it, forcing a process.exit() while the child's
  // stdout may still be flushing verdicts to the parent.
  body += 'setInterval(() => {}, 1_000_000);\n';
  for (let t = 0; t < TESTS_PER_FILE; t++) {
    body += `test('f${f}_t${t}', () => {});\n`;
  }
  writeFileSync(join(testsDir, `f${f}.test.js`), body);
}

// Write the aggregated report to a file rather than piping it through
// spawnSync (12k TAP lines would exceed the default maxBuffer). Running from
// within testsDir lets the runner auto-discover the *.test.js files.
const destination = tmpdir.resolve('out.tap');
const args = [
  '--test',
  '--test-force-exit',
  '--test-concurrency=16',
  '--test-reporter=tap',
  `--test-reporter-destination=${destination}`,
];
const child = spawnSync(process.execPath, args, { encoding: 'utf8', cwd: testsDir });

assert.strictEqual(
  child.status,
  0,
  `unexpected exit code ${child.status} (signal ${child.signal})\n${child.stderr}`,
);

const output = readFileSync(destination, 'utf8');
const pass = output.match(/# pass (\d+)/);
const fail = output.match(/# fail (\d+)/);
assert.notStrictEqual(pass, null, `missing pass summary in output:\n${output}`);
assert.strictEqual(
  Number(pass[1]),
  EXPECTED,
  `expected ${EXPECTED} passing verdicts but only ${pass[1]} reached the reporter ` +
  `(force-exit truncated the child -> parent report stream)`,
);
assert.strictEqual(Number(fail?.[1]), 0, `unexpected failures:\n${output}`);
