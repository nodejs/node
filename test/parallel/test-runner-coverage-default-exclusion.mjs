import '../common/index.mjs';
import { before, describe, it } from 'node:test';
import assert from 'node:assert';
import { cp } from 'node:fs/promises';
import { mkdtempSync, mkdirSync, rmSync } from 'node:fs';
import { tmpdir as osTmpdir } from 'node:os';
import { join } from 'node:path';
import tmpdir from '../common/tmpdir.js';
import fixtures from '../common/fixtures.js';
import { spawnSyncAndAssert } from '../common/child_process.js';
const skipIfNoInspector = {
  skip: !process.features.inspector ? 'inspector disabled' : false
};

tmpdir.refresh();

async function setupFixtures() {
  const fixtureDir = fixtures.path('test-runner', 'coverage-default-exclusion');
  await cp(fixtureDir, tmpdir.path, { recursive: true });
}

function assertDefaultExclusions(stdout) {
  assert.match(stdout, /# start of coverage report/);
  assert.doesNotMatch(stdout, /# file-test\.js\s+\|/);
  assert.doesNotMatch(stdout, /# file\.test\.mjs\s+\|/);
  assert.doesNotMatch(stdout, /# file\.test\.ts\s+\|/);
  assert.doesNotMatch(stdout, /# test\.cjs\s+\|/);
  assert.doesNotMatch(stdout, /#\s+not-matching-test-name\.js\s+\|/);
  assert.match(stdout, /# end of coverage report/);
}

describe('test runner coverage default exclusion', skipIfNoInspector, () => {
  before(async () => {
    await setupFixtures();
  });

  it('should override default exclusion setting --test-coverage-exclude', async () => {
    const report = [
      '# start of coverage report',
      '# ---------------------------------------------------------------------------',
      '# file                       | line % | branch % | funcs % | uncovered lines',
      '# ---------------------------------------------------------------------------',
      '# file-test.js               | 100.00 |   100.00 |  100.00 | ',
      '# file.test.mjs              | 100.00 |   100.00 |  100.00 | ',
      '# logic-file.js              |  66.67 |   100.00 |   50.00 | 5-7',
      '# test.cjs                   | 100.00 |   100.00 |  100.00 | ',
      '# test                       |        |          |         | ',
      '#  not-matching-test-name.js | 100.00 |   100.00 |  100.00 | ',
      '# ---------------------------------------------------------------------------',
      '# all files                  |  91.89 |   100.00 |   83.33 | ',
      '# ---------------------------------------------------------------------------',
      '# end of coverage report',
    ].join('\n');


    const args = [
      '--test',
      '--experimental-test-coverage',
      '--test-coverage-exclude=!test/**',
      '--test-reporter=tap',
      '--no-experimental-strip-types',
    ];
    spawnSyncAndAssert(process.execPath, args, {
      env: { ...process.env, NODE_TEST_TMPDIR: tmpdir.path },
      cwd: tmpdir.path
    }, {
      stderr: '',
      stdout: new RegExp(RegExp.escape(report)),
    });
  });

  it('should exclude test files from coverage by default', async () => {
    const args = [
      '--no-experimental-strip-types',
      '--test',
      '--experimental-test-coverage',
      '--test-reporter=tap',
    ];
    spawnSyncAndAssert(process.execPath, args, {
      env: { ...process.env, NODE_TEST_TMPDIR: tmpdir.path },
      cwd: tmpdir.path
    }, {
      stderr: '',
      stdout: assertDefaultExclusions,
    });
  });

  it('should exclude ts test files', async () => {
    const args = [
      '--test',
      '--experimental-test-coverage',
      '--disable-warning=ExperimentalWarning',
      '--test-reporter=tap',
    ];
    spawnSyncAndAssert(process.execPath, args, {
      env: { ...process.env, NODE_TEST_TMPDIR: tmpdir.path },
      cwd: tmpdir.path
    }, {
      stderr: '',
      stdout: assertDefaultExclusions,
    });
  });

  it('should not exclude files based on ancestor directories named "test"', async () => {
    // Regression test for https://github.com/nodejs/node/issues/58654: the
    // default coverage exclusion globs must only be evaluated against paths
    // relative to the project, never against the absolute filesystem path.
    // Otherwise, a project living anywhere underneath a directory that
    // happens to be named "test" (a container WORKDIR, a "test" home
    // directory, a CI checkout path, etc.) would have every one of its
    // files spuriously match `**/test/**/*.js` and silently disappear from
    // the coverage report, even though none of those files are actually
    // part of the project's own test suite.
    //
    // This is deliberately set up outside of `tmpdir.path` (which is nested
    // under this repository's own `test/` directory as `test/.tmp.N`)
    // because the `.` prefix on `.tmp.N` happens to block the buggy
    // absolute-path glob match by itself (globs don't cross dotfile/dotdir
    // segments unless `dot: true`), which would mask the very bug this test
    // exists to catch.
    const base = mkdtempSync(join(osTmpdir(), 'node-test-coverage-ancestor-'));
    const projectDir = join(base, 'test', 'project');
    mkdirSync(projectDir, { recursive: true });

    try {
      await cp(fixtures.path('test-runner', 'coverage-default-exclusion'), projectDir, { recursive: true });

      const args = [
        '--no-experimental-strip-types',
        '--test',
        '--experimental-test-coverage',
        '--test-reporter=tap',
      ];
      spawnSyncAndAssert(process.execPath, args, {
        env: { ...process.env, NODE_TEST_TMPDIR: tmpdir.path },
        cwd: projectDir,
      }, {
        stderr: '',
        stdout(output) {
          assertDefaultExclusions(output);
          // logic-file.js is not a test file and lives directly in the
          // project root, so it must still be reported with its real
          // (non-zero, partial) coverage numbers rather than being
          // silently excluded because an ancestor directory is named
          // "test".
          assert.match(output, /# logic-file\.js\s+\|\s*66\.67\s+\|\s*100\.00\s+\|\s*50\.00\s+\|\s*5-7/);
        },
      });
    } finally {
      rmSync(base, { recursive: true, force: true });
    }
  });

  it('should exclude a file matched by an absolute --test-coverage-exclude pattern', async () => {
    // Coverage for the isAbsolute(pattern) branch in createCoverageMatcher:
    // a pattern that is itself an absolute path must still be matched
    // against each file's absolute path, even though every relative-style
    // pattern (including all of the defaults) is now evaluated only
    // against the cwd-relative path. Passing --test-coverage-exclude
    // replaces the default exclude patterns entirely, so if the absolute
    // match didn't work, nothing would be excluded and logic-file.js would
    // show up in the report.
    const absoluteLogicFilePath = join(tmpdir.path, 'logic-file.js');
    const args = [
      '--no-experimental-strip-types',
      '--test',
      '--experimental-test-coverage',
      `--test-coverage-exclude=${absoluteLogicFilePath}`,
      '--test-reporter=tap',
    ];
    spawnSyncAndAssert(process.execPath, args, {
      env: { ...process.env, NODE_TEST_TMPDIR: tmpdir.path },
      cwd: tmpdir.path,
    }, {
      stderr: '',
      stdout(output) {
        assert.match(output, /# start of coverage report/);
        assert.doesNotMatch(output, /# logic-file\.js\s+\|/);
        assert.match(output, /# file-test\.js\s+\|/);
      },
    });
  });

  it('should include a file matched by an absolute --test-coverage-include pattern', async () => {
    // Coverage for the isAbsolute(pattern) branch on the include-glob side:
    // an absolute --test-coverage-include pattern must match by absolute
    // path. Combined with the (still relative-only) default exclude
    // patterns, only logic-file.js should end up in the report.
    const absoluteLogicFilePath = join(tmpdir.path, 'logic-file.js');
    const args = [
      '--no-experimental-strip-types',
      '--test',
      '--experimental-test-coverage',
      `--test-coverage-include=${absoluteLogicFilePath}`,
      '--test-reporter=tap',
    ];
    spawnSyncAndAssert(process.execPath, args, {
      env: { ...process.env, NODE_TEST_TMPDIR: tmpdir.path },
      cwd: tmpdir.path,
    }, {
      stderr: '',
      stdout(output) {
        assert.match(output, /# logic-file\.js\s+\|/);
        assert.doesNotMatch(output, /# file-test\.js\s+\|/);
        assert.doesNotMatch(output, /# test\.cjs\s+\|/);
      },
    });
  });

  it('should exclude dotfile test files from coverage by default', async () => {
    const args = [
      '--no-experimental-strip-types',
      '--test',
      '--experimental-test-coverage',
      '--test-reporter=tap',
      'test/.dotfile.cjs',
    ];
    spawnSyncAndAssert(process.execPath, args, {
      env: { ...process.env, NODE_TEST_TMPDIR: tmpdir.path },
      cwd: tmpdir.path
    }, {
      stderr: '',
      stdout(output) {
        assertDefaultExclusions(output);
        assert.doesNotMatch(output, /#\s+\.dotfile\.cjs\s+\|/);
      },
    });
  });
});
