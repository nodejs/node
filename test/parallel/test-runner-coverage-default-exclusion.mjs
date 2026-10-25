import '../common/index.mjs';
import { before, describe, it } from 'node:test';
import assert from 'node:assert';
import { cp } from 'node:fs/promises';
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
