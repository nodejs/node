'use strict';
const common = require('../common');
const assert = require('node:assert');
const { spawnSync } = require('node:child_process');
const { test } = require('node:test');
const fixtures = require('../common/fixtures');
const tmpdir = require('../common/tmpdir');

common.skipIfInspectorDisabled();
tmpdir.refresh();

const fixture = fixtures.path('test-runner', 'coverage.js');
const reporter = fixtures.fileURL('test-runner/custom_reporters/coverage.mjs');

test('statement coverage is reported via custom reporter', () => {
  const result = spawnSync(process.execPath, [
    '--test',
    '--experimental-test-coverage',
    '--test-coverage-exclude=!test/**',
    '--test-reporter', reporter,
    fixture,
  ]);

  assert.strictEqual(result.stderr.toString(), '');
  assert.strictEqual(result.status, 0);

  const stdout = JSON.parse(result.stdout.toString());
  const { summary } = stdout;

  // Verify statement coverage fields exist in totals
  assert.ok('coveredStatementPercent' in summary.totals,
            'totals should have coveredStatementPercent');
  assert.ok('totalStatementCount' in summary.totals,
            'totals should have totalStatementCount');
  assert.ok('coveredStatementCount' in summary.totals,
            'totals should have coveredStatementCount');

  // Statement coverage should be a number between 0 and 100
  assert.ok(summary.totals.coveredStatementPercent >= 0 &&
            summary.totals.coveredStatementPercent <= 100,
            `statement percent should be 0-100, got ${summary.totals.coveredStatementPercent}`);

  // Each file should have statement data
  for (const file of summary.files) {
    assert.ok('coveredStatementPercent' in file,
              `${file.path} should have coveredStatementPercent`);
    assert.ok('totalStatementCount' in file,
              `${file.path} should have totalStatementCount`);
    assert.ok('coveredStatementCount' in file,
              `${file.path} should have coveredStatementCount`);
    assert.ok(Array.isArray(file.statements),
              `${file.path} should have statements array`);

    // Verify statement reports have the right shape
    for (const stmt of file.statements) {
      assert.ok('line' in stmt, 'statement should have line');
      assert.ok('count' in stmt, 'statement should have count');
    }
  }
});

test('statement coverage reports covered and uncovered statements', () => {
  const result = spawnSync(process.execPath, [
    '--test',
    '--experimental-test-coverage',
    '--test-coverage-exclude=!test/**',
    '--test-reporter', reporter,
    fixture,
  ]);

  assert.strictEqual(result.status, 0);
  const { summary } = JSON.parse(result.stdout.toString());
  const coverageFile = summary.files.find((f) => f.path.endsWith('coverage.js'));

  assert.ok(coverageFile, 'coverage.js should be in the report');

  // The file has uncalled functions and dead branches, so statement
  // coverage should be less than 100%.
  assert.ok(coverageFile.coveredStatementPercent < 100,
            'coverage.js should have uncovered statements');
  assert.ok(coverageFile.coveredStatementPercent > 0,
            'coverage.js should have some covered statements');

  // Verify specific uncovered statements exist
  const uncoveredStmts = coverageFile.statements.filter((s) => s.count === 0);
  assert.ok(uncoveredStmts.length > 0,
            'should have uncovered statements');

  // The uncalled function body should have uncovered statements
  const coveredStmts = coverageFile.statements.filter((s) => s.count > 0);
  assert.ok(coveredStmts.length > 0,
            'should have covered statements');
});

test('statement coverage threshold passing', () => {
  const result = spawnSync(process.execPath, [
    '--test',
    '--experimental-test-coverage',
    '--test-coverage-exclude=!test/**',
    '--test-coverage-statements=25',
    '--test-reporter', 'tap',
    fixture,
  ]);

  const stdout = result.stdout.toString();
  assert.doesNotMatch(stdout, /Error: [\d.]+% statement coverage/);
  assert.strictEqual(result.status, 0);
});

test('statement coverage threshold failing', () => {
  const result = spawnSync(process.execPath, [
    '--test',
    '--experimental-test-coverage',
    '--test-coverage-exclude=!test/**',
    '--test-coverage-statements=99',
    '--test-reporter', 'tap',
    fixture,
  ]);

  const stdout = result.stdout.toString();
  assert.match(stdout, /Error: [\d.]+% statement coverage does not meet threshold of 99%/);
  assert.strictEqual(result.status, 1);
});

test('statement coverage threshold with custom reporter', () => {
  const result = spawnSync(process.execPath, [
    '--test',
    '--experimental-test-coverage',
    '--test-coverage-exclude=!test/**',
    '--test-coverage-statements=50',
    '--test-reporter', reporter,
    fixture,
  ]);

  const stdout = JSON.parse(result.stdout.toString());
  assert.strictEqual(stdout.summary.thresholds.statement, 50);
  assert.strictEqual(result.status, 0);
});

test('statement coverage out-of-range threshold (too high)', () => {
  const result = spawnSync(process.execPath, [
    '--test',
    '--experimental-test-coverage',
    '--test-coverage-statements=101',
    fixture,
  ]);

  assert.match(result.stderr.toString(), /The value of "--test-coverage-statements"/);
  assert.strictEqual(result.status, 1);
});

test('statement coverage out-of-range threshold (too low)', () => {
  const result = spawnSync(process.execPath, [
    '--test',
    '--experimental-test-coverage',
    '--test-coverage-statements=-1',
    fixture,
  ]);

  assert.match(result.stderr.toString(), /The value of "--test-coverage-statements"/);
  assert.strictEqual(result.status, 1);
});

test('statement coverage column appears in report', () => {
  const result = spawnSync(process.execPath, [
    '--experimental-test-coverage',
    '--test-coverage-exclude=!test/**',
    '--test-reporter', 'tap',
    fixture,
  ]);

  const stdout = result.stdout.toString();
  assert.ok(stdout.includes('stmts %'), 'report should include stmts % column');
  assert.ok(stdout.includes('start of coverage report'),
            'report should contain coverage report');
});

test('100% statement coverage for fully covered file', () => {
  const result = spawnSync(process.execPath, [
    '--test',
    '--experimental-test-coverage',
    '--test-coverage-exclude=!test/**',
    '--test-reporter', reporter,
    fixture,
  ]);

  assert.strictEqual(result.status, 0);
  const { summary } = JSON.parse(result.stdout.toString());

  // invalid-tap.js should have 100% statement coverage
  const invalidTap = summary.files.find((f) => f.path.endsWith('invalid-tap.js'));
  assert.ok(invalidTap, 'invalid-tap.js should be in the report');
  assert.strictEqual(invalidTap.coveredStatementPercent, 100);
});
