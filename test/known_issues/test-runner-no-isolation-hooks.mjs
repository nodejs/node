import * as common from '../common/index.mjs';
import * as fixtures from '../common/fixtures.mjs';
import { test } from 'node:test';

const testArguments = [
  '--test',
  '--test-isolation=none',
];

const testFiles = [
  fixtures.path('test-runner', 'no-isolation', 'one.test.js'),
  fixtures.path('test-runner', 'no-isolation', 'two.test.js'),
];

const order = [
  'before(): global',

  'before one: <root>',
  'suite one',

  'before two: <root>',
  'suite two',

  'beforeEach(): global',
  'beforeEach one: suite one - test',
  'beforeEach two: suite one - test',

  'suite one - test',
  'afterEach(): global',
  'afterEach one: suite one - test',
  'afterEach two: suite one - test',

  'before suite two: suite two',
  'beforeEach(): global',
  'beforeEach one: suite two - test',
  'beforeEach two: suite two - test',

  'suite two - test',
  'afterEach(): global',
  'afterEach one: suite two - test',
  'afterEach two: suite two - test',

  'after(): global',
  'after one: <root>',
  'after two: <root>',
].join('\n');

/**
 * TODO: The `--require` flag is processed in `loadPreloadModules` (process/pre_execution.js) BEFORE
 * the root test is created by the test runner. This causes a global `before` hook to register (and
 * run) but then the root test-case is created, causing the "subsequent" hooks to get lost. This
 * behaviour (CJS route only) is different from the ESM route, where test runner explicitly handles
 * `--import` in `root.runInAsyncScope` (test_runner/runner.js).
 * @see https://github.com/nodejs/node/pull/57595#issuecomment-2770724492
 * @see https://github.com/nodejs/node/issues/57728
 * Moved from test/parallel/test-runner-no-isolation-hooks.mjs
 */
test('use --require to define global hooks', async (t) => {
  const { stdout } = await common.spawnPromisified(process.execPath, [
    ...testArguments,
    '--require', fixtures.path('test-runner', 'no-isolation', 'global-hooks.cjs'),
    ...testFiles,
  ]);

  const testHookOutput = stdout.split('\n▶')[0];

  t.assert.equal(testHookOutput, order);
});
