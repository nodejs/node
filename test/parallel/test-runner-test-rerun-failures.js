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

const expectedStateFile = [
  {
    'test/fixtures/test-runner/rerun.js:9:1': { passed_on_attempt: 0, name: 'ok' },
    'test/fixtures/test-runner/rerun.js:17:3': { passed_on_attempt: 0, name: 'ambiguous (expectedAttempts=0)' },
    'test/fixtures/test-runner/rerun.js:30:16': { passed_on_attempt: 0, name: '2 levels deep' },
    'test/fixtures/test-runner/rerun.js:29:13': { passed_on_attempt: 0, name: 'nested' },
    'test/fixtures/test-runner/rerun.js:35:13': { passed_on_attempt: 0, name: 'ok' },
    'test/fixtures/test-runner/rerun.js:39:1': { passed_on_attempt: 0, name: 'nested ambiguous (expectedAttempts=0)' },
    'test/fixtures/test-runner/rerun.js:30:16:(1)': { passed_on_attempt: 0, name: '2 levels deep' },
    'test/fixtures/test-runner/rerun.js:35:13:(1)': { passed_on_attempt: 0, name: 'ok' },
    'test/fixtures/test-runner/rerun.js:45:13': { passed_on_attempt: 0, name: 'nested' },
    'test/fixtures/test-runner/rerun.js:44:3': { passed_on_attempt: 0, name: 'passed on first attempt' },
    'test/fixtures/test-runner/rerun.js:47:3': { passed_on_attempt: 0, name: 'a' },
    'test/fixtures/test-runner/rerun.js:43:1': { passed_on_attempt: 0, name: 'describe rerun' },
  },
  {
    'test/fixtures/test-runner/rerun.js:9:1': { passed_on_attempt: 0, name: 'ok' },
    'test/fixtures/test-runner/rerun.js:17:3': { passed_on_attempt: 0, name: 'ambiguous (expectedAttempts=0)' },
    'test/fixtures/test-runner/rerun.js:17:3:(1)': { passed_on_attempt: 1, name: 'ambiguous (expectedAttempts=1)' },
    'test/fixtures/test-runner/rerun.js:30:16': { passed_on_attempt: 0, name: '2 levels deep' },
    'test/fixtures/test-runner/rerun.js:29:13': { passed_on_attempt: 0, name: 'nested' },
    'test/fixtures/test-runner/rerun.js:35:13': { passed_on_attempt: 0, name: 'ok' },
    'test/fixtures/test-runner/rerun.js:39:1': { passed_on_attempt: 0, name: 'nested ambiguous (expectedAttempts=0)' },
    'test/fixtures/test-runner/rerun.js:30:16:(1)': { passed_on_attempt: 0, name: '2 levels deep' },
    'test/fixtures/test-runner/rerun.js:35:13:(1)': { passed_on_attempt: 0, name: 'ok' },
    'test/fixtures/test-runner/rerun.js:43:1': { passed_on_attempt: 0, name: 'describe rerun' },
    'test/fixtures/test-runner/rerun.js:44:3': { passed_on_attempt: 0, name: 'passed on first attempt' },
    'test/fixtures/test-runner/rerun.js:45:13': { passed_on_attempt: 0, name: 'nested' },
    'test/fixtures/test-runner/rerun.js:45:13:(1)': { passed_on_attempt: 1, name: 'nested' },
    'test/fixtures/test-runner/rerun.js:47:3': { passed_on_attempt: 0, name: 'a' },
  },
  {
    'test/fixtures/test-runner/rerun.js:3:1': { passed_on_attempt: 2, name: 'should fail on first two attempts' },
    'test/fixtures/test-runner/rerun.js:9:1': { passed_on_attempt: 0, name: 'ok' },
    'test/fixtures/test-runner/rerun.js:17:3': { passed_on_attempt: 0, name: 'ambiguous (expectedAttempts=0)' },
    'test/fixtures/test-runner/rerun.js:17:3:(1)': { passed_on_attempt: 1, name: 'ambiguous (expectedAttempts=1)' },
    'test/fixtures/test-runner/rerun.js:30:16': { passed_on_attempt: 0, name: '2 levels deep' },
    'test/fixtures/test-runner/rerun.js:29:13': { passed_on_attempt: 0, name: 'nested' },
    'test/fixtures/test-runner/rerun.js:35:13': { passed_on_attempt: 0, name: 'ok' },
    'test/fixtures/test-runner/rerun.js:39:1': { passed_on_attempt: 0, name: 'nested ambiguous (expectedAttempts=0)' },
    'test/fixtures/test-runner/rerun.js:29:13:(1)': { passed_on_attempt: 2, name: 'nested' },
    'test/fixtures/test-runner/rerun.js:30:16:(1)': { passed_on_attempt: 0, name: '2 levels deep' },
    'test/fixtures/test-runner/rerun.js:35:13:(1)': { passed_on_attempt: 0, name: 'ok' },
    'test/fixtures/test-runner/rerun.js:40:1': { passed_on_attempt: 2, name: 'nested ambiguous (expectedAttempts=1)' },
    'test/fixtures/test-runner/rerun.js:43:1': { passed_on_attempt: 0, name: 'describe rerun' },
    'test/fixtures/test-runner/rerun.js:44:3': { passed_on_attempt: 0, name: 'passed on first attempt' },
    'test/fixtures/test-runner/rerun.js:45:13': { passed_on_attempt: 0, name: 'nested' },
    'test/fixtures/test-runner/rerun.js:45:13:(1)': { passed_on_attempt: 1, name: 'nested' },
    'test/fixtures/test-runner/rerun.js:47:3': { passed_on_attempt: 0, name: 'a' },
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
  assert.match(stdout, /pass 11/);
  assert.match(stdout, /fail 4/);
  assert.match(stdout, /suites 1/);
  assert.deepStrictEqual(await getStateFile(), expectedStateFile.slice(0, 1));

  ({ code, stdout, signal } = await common.spawnPromisified(process.execPath, args));
  assert.strictEqual(code, 1);
  assert.strictEqual(signal, null);
  assert.match(stdout, /pass 13/);
  assert.match(stdout, /fail 3/);
  assert.match(stdout, /suites 1/);
  assert.deepStrictEqual(await getStateFile(), expectedStateFile.slice(0, 2));


  ({ code, stdout, signal } = await common.spawnPromisified(process.execPath, args));
  assert.strictEqual(code, 0);
  assert.strictEqual(signal, null);
  assert.match(stdout, /pass 18/);
  assert.match(stdout, /fail 0/);
  assert.match(stdout, /suites 1/);
  assert.deepStrictEqual(await getStateFile(), expectedStateFile);
});

test('test should pass on third rerun with `--test`', async () => {
  const args = ['--test', '--test-rerun-failures', stateFile, fixture];

  let { code, stdout, signal } = await common.spawnPromisified(process.execPath, args);
  assert.strictEqual(code, 1);
  assert.strictEqual(signal, null);
  assert.match(stdout, /pass 11/);
  assert.match(stdout, /fail 4/);
  assert.match(stdout, /suites 1/);
  assert.deepStrictEqual(await getStateFile(), expectedStateFile.slice(0, 1));

  ({ code, stdout, signal } = await common.spawnPromisified(process.execPath, args));
  assert.strictEqual(code, 1);
  assert.strictEqual(signal, null);
  assert.match(stdout, /pass 13/);
  assert.match(stdout, /fail 3/);
  assert.match(stdout, /suites 1/);
  assert.deepStrictEqual(await getStateFile(), expectedStateFile.slice(0, 2));


  ({ code, stdout, signal } = await common.spawnPromisified(process.execPath, args));
  assert.strictEqual(code, 0);
  assert.strictEqual(signal, null);
  assert.match(stdout, /pass 18/);
  assert.match(stdout, /fail 0/);
  assert.match(stdout, /suites 1/);
  assert.deepStrictEqual(await getStateFile(), expectedStateFile);
});

test('using `run` api', async () => {
  let stream = run({ files: [fixture], rerunFailuresFilePath: stateFile });
  stream.on('test:pass', common.mustCall(12));
  stream.on('test:fail', common.mustCall(4));

  // eslint-disable-next-line no-unused-vars
  for await (const _ of stream);
  await setTimeout(common.platformTimeout(10)); // Wait for the stream to finish processing
  assert.deepStrictEqual(await getStateFile(), expectedStateFile.slice(0, 1));


  stream = run({ files: [fixture], rerunFailuresFilePath: stateFile });
  stream.on('test:pass', common.mustCall(14));
  stream.on('test:fail', common.mustCall(3));

  // eslint-disable-next-line no-unused-vars
  for await (const _ of stream);
  await setTimeout(common.platformTimeout(10)); // Wait for the stream to finish processing
  assert.deepStrictEqual(await getStateFile(), expectedStateFile.slice(0, 2));


  stream = run({ files: [fixture], rerunFailuresFilePath: stateFile });
  stream.on('test:pass', common.mustCall(19));
  stream.on('test:fail', common.mustNotCall());

  // eslint-disable-next-line no-unused-vars
  for await (const _ of stream);
  await setTimeout(common.platformTimeout(10)); // Wait for the stream to finish processing
  assert.deepStrictEqual(await getStateFile(), expectedStateFile);
});
