// Regression test for https://github.com/nodejs/node/issues/64099
//
// Under the default (process) isolation, a test file whose tests are all
// filtered out by --test-name-pattern / --test-skip-pattern used to surface as
// a spurious passing entry and was counted in the "tests"/"pass" totals. It
// should instead be neither reported nor counted, matching what
// --test-isolation=none already does (see test-runner-no-isolation-filtering).
import '../common/index.mjs';
import * as fixtures from '../common/fixtures.mjs';
import assert from 'node:assert';
import { spawnSync } from 'node:child_process';
import { describe, it, run } from 'node:test';
import { tap } from 'node:test/reporters';
import consumers from 'node:stream/consumers';

const hasMatch = fixtures.path('test-runner', 'filtered-empty-files', 'has-match.test.js');
const noMatch = fixtures.path('test-runner', 'filtered-empty-files', 'no-match.test.js');
const throws = fixtures.path('test-runner', 'filtered-empty-files', 'throws.test.js');

describe('files with no tests matching the filter are not counted or reported', () => {
  it('--test-name-pattern: counts only files with a match', () => {
    const child = spawnSync(process.execPath, [
      '--test',
      '--test-reporter=tap',
      '--test-name-pattern=alpha',
      hasMatch,
      noMatch,
    ]);
    const stdout = child.stdout.toString();

    assert.strictEqual(child.status, 0);
    assert.strictEqual(child.signal, null);
    assert.match(stdout, /# tests 1/);
    assert.match(stdout, /# pass 1/);
    assert.match(stdout, /# fail 0/);
    assert.match(stdout, /ok \d+ - alpha matches/);
    // The file without a matching test must not appear at all.
    assert.doesNotMatch(stdout, /no-match\.test\.js/);
  });

  it('--test-name-pattern: a file with zero matches yields zero tests', () => {
    const child = spawnSync(process.execPath, [
      '--test',
      '--test-reporter=tap',
      '--test-name-pattern=alpha',
      noMatch,
    ]);
    const stdout = child.stdout.toString();

    assert.strictEqual(child.status, 0);
    assert.strictEqual(child.signal, null);
    assert.match(stdout, /# tests 0/);
    assert.match(stdout, /# suites 0/);
    assert.doesNotMatch(stdout, /no-match\.test\.js/);
  });

  it('--test-skip-pattern: a fully skipped file is not counted or reported', () => {
    const child = spawnSync(process.execPath, [
      '--test',
      '--test-reporter=tap',
      '--test-skip-pattern=/gamma|delta/',
      hasMatch,
      noMatch,
    ]);
    const stdout = child.stdout.toString();

    assert.strictEqual(child.status, 0);
    assert.strictEqual(child.signal, null);
    // has-match keeps both of its tests; no-match is emptied by the skip pattern.
    assert.match(stdout, /# tests 2/);
    assert.match(stdout, /# pass 2/);
    assert.doesNotMatch(stdout, /no-match\.test\.js/);
  });

  it('still reports a file that fails to load even when all its tests are filtered', () => {
    const child = spawnSync(process.execPath, [
      '--test',
      '--test-reporter=tap',
      '--test-name-pattern=alpha',
      hasMatch,
      throws,
    ]);
    const stdout = child.stdout.toString();
    const combined = stdout + child.stderr.toString();

    // A genuine load failure must surface, not be swallowed by the empty-file
    // suppression: non-zero exit, the file is reported as failing, and the
    // error is visible.
    assert.notStrictEqual(child.status, 0);
    assert.match(stdout, /# pass 1/);
    assert.match(stdout, /# fail 1/);
    assert.match(stdout, /not ok \d+ - .*throws\.test\.js/);
    assert.match(combined, /intentional load failure/);
    // The matching test in the other file is still reported.
    assert.match(stdout, /ok \d+ - alpha matches/);
  });

  it('run() API: a file with no matching tests is not counted', async () => {
    const stream = run({
      files: [hasMatch, noMatch],
      testNamePatterns: [/alpha/],
    }).compose(tap);
    const output = await consumers.text(stream);

    assert.match(output, /# tests 1/);
    assert.match(output, /ok \d+ - alpha matches/);
    assert.doesNotMatch(output, /no-match\.test\.js/);
  });
});
