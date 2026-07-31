'use strict';
// This test verifies that --test-force-exit with --test-concurrency > 1
// does not silently lose test verdicts from child processes.
// Regression test for: https://github.com/nodejs/node/issues/64833

require('../common');
const fixtures = require('../common/fixtures');
const tmpdir = require('../common/tmpdir');
const assert = require('node:assert');
const { spawnSync } = require('node:child_process');
const { readFileSync, writeFileSync, mkdirSync, rmSync } = require('node:fs');
const { resolve } = require('node:path');
const { test } = require('node:test');

tmpdir.refresh();

// Generate test files: N files × M tests each.
// We use a modest number to keep the test quick while still exercising
// the concurrent process-isolation path with force-exit.
const NUM_FILES = 8;
const TESTS_PER_FILE = 10;
const EXPECTED_TOTAL = NUM_FILES * TESTS_PER_FILE;

const testDir = tmpdir.resolve('force-exit-concurrency-tests');
rmSync(testDir, { recursive: true, force: true });
mkdirSync(testDir);

for (let f = 0; f < NUM_FILES; f++) {
  const lines = ["import { test } from 'node:test';"];
  for (let t = 0; t < TESTS_PER_FILE; t++) {
    lines.push(
      `test('file ${f} test ${t} — concurrency force-exit regression test', () => {});`
    );
  }
  writeFileSync(`${testDir}/f${f}.test.mjs`, lines.join('\n'));
}

// Custom reporter that counts leaf test verdicts.
// Uses synchronous writes to rule out reporter-side buffering.
const countReporter = tmpdir.resolve('count-reporter.mjs');
writeFileSync(countReporter, `
import fs from 'node:fs';
export default async function* countReporter(source) {
  let pass = 0, fail = 0;
  for await (const event of source) {
    if (event.type === 'test:pass' || event.type === 'test:fail') {
      if (event.data.details?.type === 'test') {
        if (event.type === 'test:pass') pass++; else fail++;
      }
    }
  }
  fs.writeSync(process.stderr.fd, JSON.stringify({ pass, fail, total: pass + fail }) + '\\n');
}
`);

test('--test-force-exit with --test-concurrency > 1 reports all tests', () => {
  const args = [
    '--test',
    '--test-force-exit',
    '--test-concurrency=4',
    '--test-reporter', countReporter,
    '--test-reporter-destination', 'stderr',
    `${testDir}/*.test.mjs`,
  ];

  // Run multiple times to catch the race.
  for (let run = 0; run < 5; run++) {
    const child = spawnSync(process.execPath, args, { encoding: 'utf8' });
    const stderr = child.stderr.toString();
    const match = stderr.match(/\{"pass":(\d+),"fail":(\d+),"total":(\d+)\}/);
    assert.ok(match, `Run ${run}: count-reporter did not produce output. stderr: ${stderr}`);
    const total = parseInt(match[3], 10);

    assert.strictEqual(
      total,
      EXPECTED_TOTAL,
      `Run ${run}: expected ${EXPECTED_TOTAL} tests reported, got ${total}. ` +
      `stderr: ${stderr.slice(0, 500)}`
    );
  }
});
