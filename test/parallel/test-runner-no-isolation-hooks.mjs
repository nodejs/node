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

  'suite one',

  'suite two',

  `before one: ${testFiles[0]}`,
  'beforeEach(): global',
  'beforeEach one: suite one - test',

  'suite one - test',
  'afterEach one: suite one - test',
  'afterEach(): global',
  `after one: ${testFiles[0]}`,

  `before two: ${testFiles[1]}`,
  'before suite two: suite two',
  'beforeEach(): global',
  'beforeEach two: suite two - test',

  'suite two - test',
  'afterEach two: suite two - test',
  'afterEach(): global',
  `after two: ${testFiles[1]}`,

  'after(): global',
].join('\n');

test('use --import (CJS) to define global hooks', async (t) => {
  const { stdout } = await common.spawnPromisified(process.execPath, [
    ...testArguments,
    '--import', fixtures.fileURL('test-runner', 'no-isolation', 'global-hooks.cjs'),
    ...testFiles,
  ]);

  const testHookOutput = stdout.split('\nafter(): global')[0] + '\nafter(): global';

  t.assert.equal(testHookOutput, order);
});

test('use --import (ESM) to define global hooks', async (t) => {
  const { stdout } = await common.spawnPromisified(process.execPath, [
    ...testArguments,
    '--import', fixtures.fileURL('test-runner', 'no-isolation', 'global-hooks.mjs'),
    ...testFiles,
  ]);

  const testHookOutput = stdout.split('\nafter(): global')[0] + '\nafter(): global';

  t.assert.equal(testHookOutput, order);
});

test('use --require to define global hooks', async (t) => {
  const { stdout } = await common.spawnPromisified(process.execPath, [
    ...testArguments,
    '--require', fixtures.path('test-runner', 'no-isolation', 'global-hooks.cjs'),
    ...testFiles,
  ]);

  const testHookOutput = stdout.split('\nafter(): global')[0] + '\nafter(): global';

  t.assert.equal(testHookOutput, order);
});
