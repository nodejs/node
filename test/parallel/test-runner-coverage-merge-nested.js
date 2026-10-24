'use strict';
// Validates that merging V8 coverage across test processes keeps never-run
// nested blocks. Without the range-tree merge, adding a second process that
// covers less can drop an uncovered nested block and make a coverage gate pass.
// See https://github.com/nodejs/node/issues/66860.
const common = require('../common');
const assert = require('node:assert');
const { spawnSync } = require('node:child_process');
const { test } = require('node:test');
const fixtures = require('../common/fixtures');
const tmpdir = require('../common/tmpdir');

common.skipIfInspectorDisabled();
tmpdir.refresh();

const fixture = fixtures.path('test-runner', 'coverage-merge-nested');

function runCoverage(...files) {
  return spawnSync(process.execPath, [
    '--test',
    '--experimental-test-coverage',
    // Clear default excludes so fixture cwd files are reported, then include
    // only mod.mjs (the shared module under test).
    '--test-coverage-exclude=!**',
    '--test-coverage-include=mod.mjs',
    '--test-coverage-branches=100',
    '--test-reporter',
    'tap',
    ...files,
  ], {
    cwd: fixture,
    env: { ...process.env, NODE_TEST_TMPDIR: tmpdir.path },
    encoding: 'utf8',
  });
}

test('single process keeps the never-run nested block uncovered', () => {
  const result = runCoverage('a.test.mjs');
  assert.match(result.stdout, /mod\.mjs\s+\|\s+[\d.]+\s+\|\s+80\.00/);
  assert.match(result.stdout, /80\.00% branch coverage does not meet threshold/);
  assert.strictEqual(result.status, 1);
});

test('merging a less-covered process must not clear the never-run nested block', () => {
  const result = runCoverage('a.test.mjs', 'b.test.mjs');
  assert.match(result.stdout, /mod\.mjs\s+\|\s+[\d.]+\s+\|\s+80\.00/);
  assert.match(result.stdout, /80\.00% branch coverage does not meet threshold/);
  assert.strictEqual(result.status, 1);
});
