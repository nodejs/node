'use strict';
const common = require('../common');

const fixtures = require('../common/fixtures');
const assert = require('node:assert');
const { rm, readFile } = require('node:fs/promises');
const { setTimeout } = require('node:timers/promises');
const { test, beforeEach, afterEach, run } = require('node:test');

const fixture = fixtures.path('test-runner', 'rerun.js');
const stateFile = fixtures.path('test-runner', 'rerun-state.json');

beforeEach(() => rm(stateFile, { force: true }));
afterEach(() => rm(stateFile, { force: true }));

const F = 'test/fixtures/test-runner/rerun.js';

const passOnceState = {
  [`${F}:9:1`]: { passed_on_attempt: 0, name: 'ok' },
  [`${F}:17:3`]: { passed_on_attempt: 0, name: 'ambiguous (expectedAttempts=0)' },
  [`${F}:39:1/${F}:29:13/${F}:30:16`]: { passed_on_attempt: 0, name: '2 levels deep' },
  [`${F}:39:1/${F}:29:13`]: { passed_on_attempt: 0, name: 'nested' },
  [`${F}:39:1/${F}:35:13`]: { passed_on_attempt: 0, name: 'ok' },
  [`${F}:39:1`]: { passed_on_attempt: 0, name: 'nested ambiguous (expectedAttempts=0)' },
  [`${F}:40:1/${F}:29:13/${F}:30:16`]: { passed_on_attempt: 0, name: '2 levels deep' },
  [`${F}:40:1/${F}:35:13`]: { passed_on_attempt: 0, name: 'ok' },
  [`${F}:43:1/${F}:44:3/${F}:45:13`]: { passed_on_attempt: 0, name: 'nested' },
  [`${F}:43:1/${F}:44:3`]: { passed_on_attempt: 0, name: 'passed on first attempt' },
  [`${F}:43:1/${F}:47:3`]: { passed_on_attempt: 0, name: 'a' },
  [`${F}:43:1`]: { passed_on_attempt: 0, name: 'describe rerun' },
  [`${F}:64:1/${F}:65:3/${F}:59:7`]: { passed_on_attempt: 0, name: 'shared sub A' },
  [`${F}:64:1/${F}:65:3/${F}:60:7`]: { passed_on_attempt: 0, name: 'shared sub B' },
  [`${F}:64:1/${F}:65:3`]: { passed_on_attempt: 0, name: 'first caller' },
  [`${F}:64:1/${F}:66:3/${F}:59:7`]: { passed_on_attempt: 0, name: 'shared sub A' },
  [`${F}:64:1/${F}:66:3/${F}:60:7`]: { passed_on_attempt: 0, name: 'shared sub B' },
  [`${F}:64:1/${F}:66:3`]: { passed_on_attempt: 0, name: 'second caller' },
  [`${F}:64:1`]: { passed_on_attempt: 0, name: 'rerun with ambiguous shared helper' },
};

const expectedStateFile = [
  { ...passOnceState },
  {
    ...passOnceState,
    [`${F}:17:3:(1)`]: { passed_on_attempt: 1, name: 'ambiguous (expectedAttempts=1)' },
  },
  {
    ...passOnceState,
    [`${F}:3:1`]: { passed_on_attempt: 2, name: 'should fail on first two attempts' },
    [`${F}:17:3:(1)`]: { passed_on_attempt: 1, name: 'ambiguous (expectedAttempts=1)' },
    [`${F}:40:1/${F}:29:13`]: { passed_on_attempt: 2, name: 'nested' },
    [`${F}:40:1`]: { passed_on_attempt: 2, name: 'nested ambiguous (expectedAttempts=1)' },
  },
];

const getStateFile = async () => {
  const res = JSON.parse((await readFile(stateFile, 'utf8')).replaceAll('\\\\', '/'));
  res.forEach((entry) => {
    for (const item in entry) {
      delete entry[item].children;
    }
  });
  return res;
};

test('test should pass on third rerun', async () => {
  const args = ['--test-rerun-failures', stateFile, fixture];

  let { code, stdout, signal } = await common.spawnPromisified(process.execPath, args);
  assert.strictEqual(code, 1);
  assert.strictEqual(signal, null);
  assert.match(stdout, /pass 17/);
  assert.match(stdout, /fail 4/);
  assert.match(stdout, /suites 2/);
  assert.deepStrictEqual(await getStateFile(), expectedStateFile.slice(0, 1));

  ({ code, stdout, signal } = await common.spawnPromisified(process.execPath, args));
  assert.strictEqual(code, 1);
  assert.strictEqual(signal, null);
  assert.match(stdout, /pass 18/);
  assert.match(stdout, /fail 3/);
  assert.match(stdout, /suites 2/);
  assert.deepStrictEqual(await getStateFile(), expectedStateFile.slice(0, 2));


  ({ code, stdout, signal } = await common.spawnPromisified(process.execPath, args));
  assert.strictEqual(code, 0);
  assert.strictEqual(signal, null);
  assert.match(stdout, /pass 21/);
  assert.match(stdout, /fail 0/);
  assert.match(stdout, /suites 2/);
  assert.deepStrictEqual(await getStateFile(), expectedStateFile);
});

test('test should pass on third rerun with `--test`', async () => {
  const args = ['--test', '--test-rerun-failures', stateFile, fixture];

  let { code, stdout, signal } = await common.spawnPromisified(process.execPath, args);
  assert.strictEqual(code, 1);
  assert.strictEqual(signal, null);
  assert.match(stdout, /pass 17/);
  assert.match(stdout, /fail 4/);
  assert.match(stdout, /suites 2/);
  assert.deepStrictEqual(await getStateFile(), expectedStateFile.slice(0, 1));

  ({ code, stdout, signal } = await common.spawnPromisified(process.execPath, args));
  assert.strictEqual(code, 1);
  assert.strictEqual(signal, null);
  assert.match(stdout, /pass 18/);
  assert.match(stdout, /fail 3/);
  assert.match(stdout, /suites 2/);
  assert.deepStrictEqual(await getStateFile(), expectedStateFile.slice(0, 2));


  ({ code, stdout, signal } = await common.spawnPromisified(process.execPath, args));
  assert.strictEqual(code, 0);
  assert.strictEqual(signal, null);
  assert.match(stdout, /pass 21/);
  assert.match(stdout, /fail 0/);
  assert.match(stdout, /suites 2/);
  assert.deepStrictEqual(await getStateFile(), expectedStateFile);
});

test('failures from shared-location subtests are not swallowed on retry', async () => {
  const crossParentFixture = fixtures.path('test-runner', 'rerun-cross-parent-subtests.js');
  const args = ['--test-rerun-failures', stateFile, crossParentFixture];

  let { code, stdout, signal } = await common.spawnPromisified(process.execPath, args);
  assert.strictEqual(signal, null);
  assert.strictEqual(code, 1);
  assert.match(stdout, /fail 2/);

  ({ code, stdout, signal } = await common.spawnPromisified(process.execPath, args));
  assert.strictEqual(signal, null);
  assert.notStrictEqual(code, 0);
  assert.doesNotMatch(stdout, /fail 0/, 'B fails -> inner must not be silently passed');
});

test('using `run` api', async () => {
  let stream = run({ files: [fixture], rerunFailuresFilePath: stateFile });
  stream.on('test:pass', common.mustCall(19));
  stream.on('test:fail', common.mustCall(4));

  // eslint-disable-next-line no-unused-vars
  for await (const _ of stream);
  await setTimeout(common.platformTimeout(10)); // Wait for the stream to finish processing
  assert.deepStrictEqual(await getStateFile(), expectedStateFile.slice(0, 1));


  stream = run({ files: [fixture], rerunFailuresFilePath: stateFile });
  stream.on('test:pass', common.mustCall(20));
  stream.on('test:fail', common.mustCall(3));

  // eslint-disable-next-line no-unused-vars
  for await (const _ of stream);
  await setTimeout(common.platformTimeout(10)); // Wait for the stream to finish processing
  assert.deepStrictEqual(await getStateFile(), expectedStateFile.slice(0, 2));


  stream = run({ files: [fixture], rerunFailuresFilePath: stateFile });
  stream.on('test:pass', common.mustCall(23));
  stream.on('test:fail', common.mustNotCall());

  // eslint-disable-next-line no-unused-vars
  for await (const _ of stream);
  await setTimeout(common.platformTimeout(10)); // Wait for the stream to finish processing
  assert.deepStrictEqual(await getStateFile(), expectedStateFile);
});
