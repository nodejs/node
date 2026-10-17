'use strict';
const common = require('../common');
common.skipIfInspectorDisabled();

const assert = require('node:assert');
const { test } = require('node:test');
const fixtures = require('../common/fixtures');

const cwd = fixtures.path('test-runner', 'coverage-isolation-none');
const env = { ...process.env, FORCE_COLOR: '0' };
delete env.NODE_V8_COVERAGE;
delete env.NODE_TEST_CONTEXT;

function assertTimingLog(stderr) {
  const lines = stderr.split('\n').filter((line) => line.includes('coverage report generation:'));
  assert.strictEqual(lines.length, 1, stderr);
  const match = /^TEST_RUNNER \d+ coverage report generation: (.*)$/.exec(lines[0]);
  assert.ok(match, stderr);
  // Accept the timer's units without imposing a limit on the elapsed time.
  assert.match(match[1], /^(?:\d+(?:\.\d+)?(?:ms|s)|\d+(?::\d{2}){1,2}\.\d{3} \((?:h:m)?m:ss\.mmm\))$/);
}

for (const isolation of ['process', 'none']) {
  for (const [coverage, debug] of [[true, true], [true, false], [false, true]]) {
    test(`coverage timing: isolation=${isolation}, coverage=${coverage}, debug=${debug}`, async () => {
      const args = ['--test', `--test-isolation=${isolation}`, '--test-reporter=tap'];
      if (coverage) args.push('--experimental-test-coverage');
      args.push('tests/foo.test.mjs');

      const { code, signal, stdout, stderr } = await common.spawnPromisified(process.execPath, args, {
        cwd,
        env: { ...env, NODE_DEBUG: debug ? 'test_runner' : '' },
      });
      assert.strictEqual(code, 0, stderr);
      assert.strictEqual(signal, null);
      assert.match(stdout, /# pass 2/);
      assert.doesNotMatch(stdout, /coverage report generation:/);
      if (coverage) assert.match(stdout, /# start of coverage report/);

      if (coverage && debug) {
        assertTimingLog(stderr);
      } else {
        assert.doesNotMatch(stderr, /coverage report generation:/);
        if (!debug) assert.strictEqual(stderr, '');
      }
    });
  }
}

test('coverage timing ends when summary generation fails and cleanup still runs', async () => {
  const { code, signal, stdout, stderr } = await common.spawnPromisified(process.execPath, [
    '--expose-internals',
    '--experimental-test-coverage',
    '--test-reporter=tap',
    '-e', `
      const assert = require('node:assert');
      const { TestCoverage } = require('internal/test_runner/coverage');
      const { test } = require('node:test');
      TestCoverage.prototype.summary = () => { throw new Error('summary failed'); };
      test('passing test', () => {});
      // The real cleanup restores NODE_V8_COVERAGE after reporting the error.
      process.on('exit', () => assert.strictEqual(process.env.NODE_V8_COVERAGE, undefined));
    `,
  ], { env: { ...env, NODE_DEBUG: 'test_runner' } });

  assert.strictEqual(code, 1, stderr);
  assert.strictEqual(signal, null);
  assert.match(stdout, /Warning: Could not report code coverage\. Error: summary failed/);
  assert.doesNotMatch(stdout, /Could not clean up code coverage/);
  assertTimingLog(stderr);
  assert.doesNotMatch(stderr, /No such label|already exists|AssertionError/);
});
