'use strict';
const common = require('../common');

const fixtures = require('../common/fixtures');
const assert = require('node:assert');
const { rm, readFile } = require('node:fs/promises');
const { test, beforeEach, afterEach } = require('node:test');

const stateFile = fixtures.path('test-runner', 'rerun-state.json');

beforeEach(() => rm(stateFile, { force: true }));
afterEach(() => rm(stateFile, { force: true }));

test('test should pass on third rerun', async () => {
  const fixture = fixtures.path('test-runner', 'rerun.js');
  const args = ['--test-rerun-failures', stateFile, fixture];

  let { code, stdout, signal } = await common.spawnPromisified(process.execPath, args);
  assert.strictEqual(code, 1);
  assert.strictEqual(signal, null);
  assert.match(stdout, /pass 2/);
  assert.match(stdout, /fail 2/);
  assert.deepStrictEqual(JSON.parse(await readFile(stateFile, 'utf8')), [
    {
      'test/fixtures/test-runner/rerun.js:17:3': { passed_on_attempt: 0, name: 'ambiguous (expectedAttempts=0)' },
      'test/fixtures/test-runner/rerun.js:9:1': { passed_on_attempt: 0, name: 'ok' },
    },
  ]);

  ({ code, stdout, signal } = await common.spawnPromisified(process.execPath, args));
  assert.strictEqual(code, 1);
  assert.strictEqual(signal, null);
  assert.match(stdout, /pass 3/);
  assert.match(stdout, /fail 1/);
  assert.deepStrictEqual(JSON.parse(await readFile(stateFile, 'utf8')), [
    {
      'test/fixtures/test-runner/rerun.js:17:3': { passed_on_attempt: 0, name: 'ambiguous (expectedAttempts=0)' },
      'test/fixtures/test-runner/rerun.js:9:1': { passed_on_attempt: 0, name: 'ok' },
    },
    {
      'test/fixtures/test-runner/rerun.js:17:3': { passed_on_attempt: 0, name: 'ambiguous (expectedAttempts=0)' },
      'test/fixtures/test-runner/rerun.js:17:3:(1)': { passed_on_attempt: 1, name: 'ambiguous (expectedAttempts=1)' },
      'test/fixtures/test-runner/rerun.js:9:1': { passed_on_attempt: 0, name: 'ok' },
    },
  ]);


  ({ code, stdout, signal } = await common.spawnPromisified(process.execPath, args));
  assert.strictEqual(code, 0);
  assert.strictEqual(signal, null);
  assert.match(stdout, /pass 4/);
  assert.match(stdout, /fail 0/);
  assert.deepStrictEqual(JSON.parse(await readFile(stateFile, 'utf8')), [
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
  ]);
});
