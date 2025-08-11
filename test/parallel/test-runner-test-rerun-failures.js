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
    'test/fixtures/test-runner/rerun.js:17:3': { passed_on_attempt: 0, name: 'ambiguous (expectedAttempts=0)' },
    'test/fixtures/test-runner/rerun.js:9:1': { passed_on_attempt: 0, name: 'ok' },
  },
  {
    'test/fixtures/test-runner/rerun.js:17:3': { passed_on_attempt: 0, name: 'ambiguous (expectedAttempts=0)' },
    'test/fixtures/test-runner/rerun.js:17:3:(1)': { passed_on_attempt: 1, name: 'ambiguous (expectedAttempts=1)' },
    'test/fixtures/test-runner/rerun.js:9:1': { passed_on_attempt: 0, name: 'ok' },
  },
  {
    'test/fixtures/test-runner/rerun.js:17:3': { passed_on_attempt: 0, name: 'ambiguous (expectedAttempts=0)' },
    'test/fixtures/test-runner/rerun.js:17:3:(1)': { passed_on_attempt: 1, name: 'ambiguous (expectedAttempts=1)' },
    'test/fixtures/test-runner/rerun.js:9:1': { passed_on_attempt: 0, name: 'ok' },
    'test/fixtures/test-runner/rerun.js:3:1': { passed_on_attempt: 2, name: 'should fail on first two attempts' },
  },
];

const getStateFile = async () => JSON.parse((await readFile(stateFile, 'utf8')).replaceAll('\\\\', '/'));

test('test should pass on third rerun', async () => {
  const args = ['--test-rerun-failures', stateFile, fixture];

  let { code, stdout, signal } = await common.spawnPromisified(process.execPath, args);
  assert.strictEqual(code, 1);
  assert.strictEqual(signal, null);
  assert.match(stdout, /pass 2/);
  assert.match(stdout, /fail 2/);
  assert.deepStrictEqual(await getStateFile(), expectedStateFile.slice(0, 1));

  ({ code, stdout, signal } = await common.spawnPromisified(process.execPath, args));
  assert.strictEqual(code, 1);
  assert.strictEqual(signal, null);
  assert.match(stdout, /pass 3/);
  assert.match(stdout, /fail 1/);
  assert.deepStrictEqual(await getStateFile(), expectedStateFile.slice(0, 2));


  ({ code, stdout, signal } = await common.spawnPromisified(process.execPath, args));
  assert.strictEqual(code, 0);
  assert.strictEqual(signal, null);
  assert.match(stdout, /pass 4/);
  assert.match(stdout, /fail 0/);
  assert.deepStrictEqual(await getStateFile(), expectedStateFile);
});

test('test should pass on third rerun with `--test`', async () => {
  const args = ['--test', '--test-rerun-failures', stateFile, fixture];

  let { code, stdout, signal } = await common.spawnPromisified(process.execPath, args);
  assert.strictEqual(code, 1);
  assert.strictEqual(signal, null);
  assert.match(stdout, /pass 2/);
  assert.match(stdout, /fail 2/);
  assert.deepStrictEqual(await getStateFile(), expectedStateFile.slice(0, 1));

  ({ code, stdout, signal } = await common.spawnPromisified(process.execPath, args));
  assert.strictEqual(code, 1);
  assert.strictEqual(signal, null);
  assert.match(stdout, /pass 3/);
  assert.match(stdout, /fail 1/);
  assert.deepStrictEqual(await getStateFile(), expectedStateFile.slice(0, 2));


  ({ code, stdout, signal } = await common.spawnPromisified(process.execPath, args));
  assert.strictEqual(code, 0);
  assert.strictEqual(signal, null);
  assert.match(stdout, /pass 4/);
  assert.match(stdout, /fail 0/);
  assert.deepStrictEqual(await getStateFile(), expectedStateFile);
});

test('using `run` api', async () => {
  let stream = run({ files: [fixture], rerunFailuresFilePath: stateFile });
  stream.on('test:pass', common.mustCall(2));
  stream.on('test:fail', common.mustCall(2));

  // eslint-disable-next-line no-unused-vars
  for await (const _ of stream);
  await setTimeout(common.platformTimeout(10)); // Wait for the stream to finish processing
  assert.deepStrictEqual(await getStateFile(), expectedStateFile.slice(0, 1));


  stream = run({ files: [fixture], rerunFailuresFilePath: stateFile });
  stream.on('test:pass', common.mustCall(3));
  stream.on('test:fail', common.mustCall(1));

  // eslint-disable-next-line no-unused-vars
  for await (const _ of stream);
  await setTimeout(common.platformTimeout(10)); // Wait for the stream to finish processing
  assert.deepStrictEqual(await getStateFile(), expectedStateFile.slice(0, 2));


  stream = run({ files: [fixture], rerunFailuresFilePath: stateFile });
  stream.on('test:pass', common.mustCall(4));
  stream.on('test:fail', common.mustNotCall());

  // eslint-disable-next-line no-unused-vars
  for await (const _ of stream);
  await setTimeout(common.platformTimeout(10)); // Wait for the stream to finish processing
  assert.deepStrictEqual(await getStateFile(), expectedStateFile);
});
