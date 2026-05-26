'use strict';
const common = require('../common');
const tmpdir = require('../common/tmpdir');

const fixtures = require('../common/fixtures');
const assert = require('node:assert');
const { rm, readFile } = require('node:fs/promises');
const { relative } = require('node:path');
const { setTimeout } = require('node:timers/promises');
const { test, beforeEach, afterEach, run } = require('node:test');

const fixture = fixtures.path('test-runner', 'rerun.js');
tmpdir.refresh();
const stateFile = tmpdir.resolve('rerun-state.json');

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
    'test/fixtures/test-runner/rerun.js:59:7': { passed_on_attempt: 0, name: 'shared sub A' },
    'test/fixtures/test-runner/rerun.js:60:7': { passed_on_attempt: 0, name: 'shared sub B' },
    'test/fixtures/test-runner/rerun.js:65:3': { passed_on_attempt: 0, name: 'first caller' },
    'test/fixtures/test-runner/rerun.js:59:7:(1)': { passed_on_attempt: 0, name: 'shared sub A' },
    'test/fixtures/test-runner/rerun.js:60:7:(1)': { passed_on_attempt: 0, name: 'shared sub B' },
    'test/fixtures/test-runner/rerun.js:66:3': { passed_on_attempt: 0, name: 'second caller' },
    'test/fixtures/test-runner/rerun.js:64:1': { passed_on_attempt: 0, name: 'rerun with ambiguous shared helper' },
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
    'test/fixtures/test-runner/rerun.js:45:13': { passed_on_attempt: 0, name: 'nested' },
    'test/fixtures/test-runner/rerun.js:44:3': { passed_on_attempt: 0, name: 'passed on first attempt' },
    'test/fixtures/test-runner/rerun.js:47:3': { passed_on_attempt: 0, name: 'a' },
    'test/fixtures/test-runner/rerun.js:43:1': { passed_on_attempt: 0, name: 'describe rerun' },
    'test/fixtures/test-runner/rerun.js:59:7': { passed_on_attempt: 0, name: 'shared sub A' },
    'test/fixtures/test-runner/rerun.js:60:7': { passed_on_attempt: 0, name: 'shared sub B' },
    'test/fixtures/test-runner/rerun.js:65:3': { passed_on_attempt: 0, name: 'first caller' },
    'test/fixtures/test-runner/rerun.js:59:7:(1)': { passed_on_attempt: 0, name: 'shared sub A' },
    'test/fixtures/test-runner/rerun.js:60:7:(1)': { passed_on_attempt: 0, name: 'shared sub B' },
    'test/fixtures/test-runner/rerun.js:66:3': { passed_on_attempt: 0, name: 'second caller' },
    'test/fixtures/test-runner/rerun.js:64:1': { passed_on_attempt: 0, name: 'rerun with ambiguous shared helper' },
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
    'test/fixtures/test-runner/rerun.js:30:16:(1)': { passed_on_attempt: 0, name: '2 levels deep' },
    'test/fixtures/test-runner/rerun.js:29:13:(1)': { passed_on_attempt: 2, name: 'nested' },
    'test/fixtures/test-runner/rerun.js:35:13:(1)': { passed_on_attempt: 0, name: 'ok' },
    'test/fixtures/test-runner/rerun.js:40:1': { passed_on_attempt: 2, name: 'nested ambiguous (expectedAttempts=1)' },
    'test/fixtures/test-runner/rerun.js:45:13': { passed_on_attempt: 0, name: 'nested' },
    'test/fixtures/test-runner/rerun.js:44:3': { passed_on_attempt: 0, name: 'passed on first attempt' },
    'test/fixtures/test-runner/rerun.js:47:3': { passed_on_attempt: 0, name: 'a' },
    'test/fixtures/test-runner/rerun.js:43:1': { passed_on_attempt: 0, name: 'describe rerun' },
    'test/fixtures/test-runner/rerun.js:59:7': { passed_on_attempt: 0, name: 'shared sub A' },
    'test/fixtures/test-runner/rerun.js:60:7': { passed_on_attempt: 0, name: 'shared sub B' },
    'test/fixtures/test-runner/rerun.js:65:3': { passed_on_attempt: 0, name: 'first caller' },
    'test/fixtures/test-runner/rerun.js:59:7:(1)': { passed_on_attempt: 0, name: 'shared sub A' },
    'test/fixtures/test-runner/rerun.js:60:7:(1)': { passed_on_attempt: 0, name: 'shared sub B' },
    'test/fixtures/test-runner/rerun.js:66:3': { passed_on_attempt: 0, name: 'second caller' },
    'test/fixtures/test-runner/rerun.js:64:1': { passed_on_attempt: 0, name: 'rerun with ambiguous shared helper' },
  },
];

const getStateFile = async () => {
  const res = JSON.parse((await readFile(stateFile, 'utf8')).replaceAll('\\\\', '/'));
  res.forEach((entry) => {
    for (const item in entry) {
      delete entry[item].children;
      delete entry[item].duration_ms;
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

test('failing test is not swallowed when siblings share its source location', async () => {
  // Regression coverage for https://github.com/nodejs/node/issues/63424.
  //
  // The fixture runs the 4-sibling group (D, E pass; F fails; G passes)
  // before the 3-sibling group from the issue (A pass, B fails, C pass) so
  // that the passing sibling at global position 2 (E) ends up recorded at
  // base:(1). With the runner-side off-by-one reintroduced, the buggy
  // counter on retry collides every sibling after the first onto base:(1)
  // and matches the failing F (and B) against E's "passed" entry, replacing
  // their assert.fail with a synthetic noop and reporting exit 0.
  //
  // The state-file shape additionally pins down the writer-side bugs: the
  // writer off-by-one or its prior pass-only counting both shift the
  // recorded slots away from the expected base:(N) layout.
  const fixturePath = fixtures.path('test-runner', 'rerun-shared-helper-swallows-failure.mjs');
  const args = ['--test-rerun-failures', stateFile, fixturePath];

  // getStateFile() normalises backslashes to '/'; match that on Windows.
  const fixtureKey = relative(process.cwd(), fixturePath).replaceAll('\\', '/');
  const innerLoc = `${fixtureKey}:25:13`;
  const passingInner = { passed_on_attempt: 0, name: 'inner' };
  const expectedInnerSlots = {
    [innerLoc]: passingInner,                  // D
    [`${innerLoc}:(1)`]: passingInner,         // E   - fills the slot a buggy runner aliases F/B onto
    [`${innerLoc}:(3)`]: passingInner,         // G   - :(2) reserved for F's failure
    [`${innerLoc}:(4)`]: passingInner,         // A
    [`${innerLoc}:(6)`]: passingInner,         // C   - :(5) reserved for B's failure
  };
  const passingParents = {
    [`${fixtureKey}:37:3`]: 'D passes',
    [`${fixtureKey}:38:3`]: 'E passes',
    [`${fixtureKey}:40:3`]: 'G passes',
    [`${fixtureKey}:48:3`]: 'A passes',
    [`${fixtureKey}:50:3`]: 'C passes',
  };
  const failingParentSlots = [
    `${fixtureKey}:39:3`,                      // F fails
    `${fixtureKey}:49:3`,                      // B fails
  ];
  const failingInnerSlots = [`${innerLoc}:(2)`, `${innerLoc}:(5)`];

  function assertAttempt(state, attemptIndex) {
    for (const [key, expected] of Object.entries(expectedInnerSlots)) {
      assert.deepStrictEqual(state[attemptIndex][key], expected, `attempt ${attemptIndex} missing or wrong entry for ${key}`);
    }
    for (const [key, name] of Object.entries(passingParents)) {
      assert.strictEqual(state[attemptIndex][key]?.name, name, `attempt ${attemptIndex} missing parent ${name} at ${key}`);
      assert.strictEqual(state[attemptIndex][key]?.passed_on_attempt, 0);
    }
    for (const key of failingInnerSlots) {
      assert.strictEqual(state[attemptIndex][key], undefined, `attempt ${attemptIndex} should not record failing inner at ${key}`);
    }
    for (const key of failingParentSlots) {
      assert.strictEqual(state[attemptIndex][key], undefined, `attempt ${attemptIndex} should not record failing parent at ${key}`);
    }
  }

  // Attempt 0: F and B fail.
  let { code, signal } = await common.spawnPromisified(process.execPath, args);
  assert.strictEqual(code, 1);
  assert.strictEqual(signal, null);
  let state = await getStateFile();
  assert.strictEqual(state.length, 1);
  assertAttempt(state, 0);

  // Attempt 1: F and B must STILL fail. Before the fix this run reported
  // exit 0 because the failing tests were matched to passing siblings'
  // previous-attempt slots and replaced with synthetic noops.
  ({ code, signal } = await common.spawnPromisified(process.execPath, args));
  assert.strictEqual(code, 1);
  assert.strictEqual(signal, null);
  state = await getStateFile();
  assert.strictEqual(state.length, 2);
  assertAttempt(state, 1);
});

test('rerun preserves the original duration on the replayed pass', async () => {
  const durationFixture = fixtures.path('test-runner', 'rerun-duration.js');
  const args = ['--test-rerun-failures', stateFile, durationFixture];

  const first = await common.spawnPromisified(process.execPath, args);
  assert.doesNotMatch(first.stdout, /passed on attempt/,
                      'no replay marker should appear on the initial run');

  const second = await common.spawnPromisified(process.execPath, args);
  assert.match(second.stdout, /passing slow test[^\n]*\(passed on attempt 0\)/,
               'spec reporter should mark the replayed test on the retry');
  assert.doesNotMatch(second.stdout, /always failing[^\n]*\(passed on attempt/,
                      'the failing test must not show the replay marker');

  const raw = JSON.parse(await readFile(stateFile, 'utf8'));
  const passKey = Object.keys(raw[0]).find((k) => raw[0][k].name === 'passing slow test');
  assert.ok(passKey, 'expected the passing test to be recorded on attempt 0');
  assert.ok(raw[0][passKey].duration_ms > 0, 'expected a measurable duration on attempt 0');
  assert.strictEqual(raw[1][passKey].duration_ms, raw[0][passKey].duration_ms);
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
