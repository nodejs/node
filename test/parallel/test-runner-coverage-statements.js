'use strict';
const common = require('../common');
const assert = require('node:assert');
const { spawnSync } = require('node:child_process');
const { test } = require('node:test');
const fixtures = require('../common/fixtures');

common.skipIfInspectorDisabled();

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

  // Regression: totalStatementCount must be > 0 for files with actual
  // statements. A zero here means the AST visitor is not firing.
  assert.ok(summary.totals.totalStatementCount > 0,
            `totalStatementCount must be > 0, got ${summary.totals.totalStatementCount}`);

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

  // Regression: totalStatementCount must be > 0 for files with actual
  // statements. A zero here means the AST visitor is not firing.
  assert.ok(coverageFile.totalStatementCount > 0,
            `totalStatementCount must be > 0, got ${coverageFile.totalStatementCount}`);

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

// Threshold passing/failing/out-of-range tests are in
// test-runner-coverage-thresholds.js via the data-driven coverages loop.

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

test('statement coverage differs from line coverage on multistatement lines', () => {
  const multiFixture = fixtures.path('test-runner', 'coverage-multistatement.js');
  const result = spawnSync(process.execPath, [
    '--test',
    '--experimental-test-coverage',
    '--test-coverage-exclude=!test/**',
    '--test-reporter', reporter,
    multiFixture,
  ]);

  assert.strictEqual(result.stderr.toString(), '');
  assert.strictEqual(result.status, 0);

  const { summary } = JSON.parse(result.stdout.toString());
  const file = summary.files.find(
    (f) => f.path.endsWith('coverage-multistatement.js'),
  );

  assert.ok(file, 'coverage-multistatement.js should be in the report');
  assert.ok(file.totalStatementCount > 0,
            'should have statements');

  // The key assertion: statement coverage should be LOWER than line
  // coverage because multiple statements share single lines where
  // only some execute (e.g. early returns, single-line if/else).
  assert.ok(file.coveredStatementPercent < file.coveredLinePercent,
            `statement % (${file.coveredStatementPercent}) should be ` +
            `lower than line % (${file.coveredLinePercent})`);

  // The uncalled function has statements that are never executed
  const uncoveredStmts = file.statements.filter((s) => s.count === 0);
  assert.ok(uncoveredStmts.length > 0,
            'should have uncovered statements from skipped branches and neverCalled');
});

test('shebang files get real statement coverage with allowHashBang', () => {
  const shebangFixture = fixtures.path('test-runner', 'coverage-shebang.js');
  const result = spawnSync(process.execPath, [
    '--test',
    '--experimental-test-coverage',
    '--test-coverage-exclude=!test/**',
    '--test-reporter', reporter,
    shebangFixture,
  ]);

  assert.strictEqual(result.stderr.toString(), '');
  assert.strictEqual(result.status, 0);

  const { summary } = JSON.parse(result.stdout.toString());
  const file = summary.files.find(
    (f) => f.path.endsWith('coverage-shebang.js'),
  );

  assert.ok(file, 'coverage-shebang.js should be in the report');

  // With allowHashBang: true, acorn parses the file successfully
  // and totalStatementCount should be > 0 (not degraded to null).
  assert.ok(file.totalStatementCount > 0,
            `shebang file must have statements, got ${file.totalStatementCount}`);

  // The farewell() function is uncalled, so coverage should be < 100%
  assert.ok(file.coveredStatementPercent < 100,
            'should have uncovered statements from farewell()');
  assert.ok(file.coveredStatementPercent > 0,
            'should have covered statements from greet()');
});

test('unparseable files degrade gracefully to zero statements', () => {
  const unparseableTest = fixtures.path(
    'test-runner', 'coverage-unparseable-test.js',
  );
  const result = spawnSync(process.execPath, [
    '--test',
    '--experimental-test-coverage',
    '--test-coverage-exclude=!test/**',
    '--test-reporter', reporter,
    unparseableTest,
  ]);

  assert.strictEqual(result.stderr.toString(), '');
  assert.strictEqual(result.status, 0);

  const { summary } = JSON.parse(result.stdout.toString());
  const file = summary.files.find(
    (f) => f.path.endsWith('coverage-unparseable.js'),
  );

  assert.ok(file, 'coverage-unparseable.js should be in the report');

  // The file uses top-level `using` which is valid at runtime (CJS wrapper)
  // but acorn cannot parse as sourceType:'script'. Statement coverage
  // degrades to 0 total statements while other metrics still work.
  assert.strictEqual(file.totalStatementCount, 0);
  assert.strictEqual(file.coveredStatementCount, 0);
  assert.strictEqual(file.coveredStatementPercent, 100);
  assert.ok(Array.isArray(file.statements) && file.statements.length === 0,
            'unparseable file should have empty statements array');

  // Line and function coverage should still work normally
  assert.ok(file.totalLineCount > 0,
            'line coverage should still work for unparseable files');
});

test('statement coverage counts class and function declarations', () => {
  const classFixture = fixtures.path('test-runner', 'coverage-class.js');
  const result = spawnSync(process.execPath, [
    '--test',
    '--experimental-test-coverage',
    '--test-coverage-exclude=!test/**',
    '--test-reporter', reporter,
    classFixture,
  ]);

  assert.strictEqual(result.stderr.toString(), '');
  assert.strictEqual(result.status, 0);

  const { summary } = JSON.parse(result.stdout.toString());
  const classFile = summary.files.find(
    (f) => f.path.endsWith('coverage-class.js'),
  );

  assert.ok(classFile, 'coverage-class.js should be in the report');
  assert.ok(classFile.totalStatementCount > 0,
            `totalStatementCount must be > 0, got ${classFile.totalStatementCount}`);

  // The file has an unused class, so statement coverage should be < 100%.
  assert.ok(classFile.coveredStatementPercent < 100,
            'should have uncovered statements from UnusedClass');
  assert.ok(classFile.coveredStatementPercent > 0,
            'should have covered statements from Dog/Animal');
});
