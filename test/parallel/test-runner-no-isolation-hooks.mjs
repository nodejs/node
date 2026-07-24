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

// Each file is wrapped in an implicit suite when using isolation: 'none',
// so file-level hooks are scoped to their file. Global hooks (registered
// via --import/--require) still run for all tests.
const file1 = fixtures.path('test-runner', 'no-isolation', 'one.test.js');
const file2 = fixtures.path('test-runner', 'no-isolation', 'two.test.js');
const order = [
  'before(): global',
  'suite one',
  'suite two',

  `before one: ${file1}`,
  'beforeEach(): global',
  'beforeEach one: suite one - test',
  'suite one - test',
  'afterEach one: suite one - test',
  'afterEach(): global',
  `after one: ${file1}`,

  `before two: ${file2}`,
  'before suite two: suite two',
  'beforeEach(): global',
  'beforeEach two: suite two - test',
  'suite two - test',
  'afterEach two: suite two - test',
  'afterEach(): global',
  `after two: ${file2}`,

  'after(): global',
].join('\n');

test('use --import (CJS) to define global hooks', async (t) => {
  const { stdout } = await common.spawnPromisified(process.execPath, [
    ...testArguments,
    '--import', fixtures.fileURL('test-runner', 'no-isolation', 'global-hooks.cjs'),
    ...testFiles,
  ]);

  const testHookOutput = stdout.split('\n▶')[0];

  t.assert.equal(testHookOutput, order);
});

test('use --import (ESM) to define global hooks', async (t) => {
  const { stdout } = await common.spawnPromisified(process.execPath, [
    ...testArguments,
    '--import', fixtures.fileURL('test-runner', 'no-isolation', 'global-hooks.mjs'),
    ...testFiles,
  ]);

  const testHookOutput = stdout.split('\n▶')[0];

  t.assert.equal(testHookOutput, order);
});

test('use --require to define global hooks', async (t) => {
  const { stdout } = await common.spawnPromisified(process.execPath, [
    ...testArguments,
    '--require', fixtures.path('test-runner', 'no-isolation', 'global-hooks.cjs'),
    ...testFiles,
  ]);

  const testHookOutput = stdout.split('\n▶')[0];

  t.assert.equal(testHookOutput, order);
});
