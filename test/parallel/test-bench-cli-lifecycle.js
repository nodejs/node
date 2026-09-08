'use strict';

const common = require('../common');
const assert = require('assert');
const { spawnSync } = require('child_process');
const fixtures = require('../common/fixtures');

const spawnTimeout = common.platformTimeout(30_000);

function spawnNode(args, options = undefined) {
  const result = spawnSync(process.execPath, args, {
    __proto__: null,
    encoding: 'utf8',
    timeout: spawnTimeout,
    ...options,
  });
  assert.ifError(result.error);
  assert.strictEqual(result.signal, null);
  return result;
}

function spawnBench(args, options = undefined) {
  return spawnNode([
    '--no-warnings', '--experimental-bench', '--bench', ...args,
  ], options);
}

function parseRecords(result) {
  assert.strictEqual(result.stderr, '');
  return parseOutput(result.stdout);
}

function parseOutput(output) {
  return output.trim().split('\n').map((line) => JSON.parse(line));
}

{
  const result = spawnBench([
    '--bench-reporter=json',
    fixtures.path('bench-runner/error.cjs'),
  ]);
  assert.strictEqual(result.status, 1);
  const records = parseRecords(result);
  const failure = records.find(
    ({ type }) => type === 'bench:complete').data;
  assert.deepStrictEqual(failure.params, { kind: 'structured-error' });
  assert.strictEqual(failure.error.name, 'Error');
  assert.strictEqual(failure.error.message, 'benchmark fixture failed');
  assert.strictEqual(failure.error.code, 'ERR_BENCHMARK_FIXTURE');
  assert.deepStrictEqual(failure.error.cause, { value: '42' });
  assert.match(failure.error.stack, /error\.cjs/);
  assert.strictEqual(records.at(-1).data.success, false);
}

{
  const result = spawnBench([
    '--bench-isolation=none',
    '--bench-reporter=json',
    fixtures.path('bench-runner/throws-null.cjs'),
  ]);
  assert.strictEqual(result.status, 1);
  const records = parseRecords(result);
  const diagnostic = records.find(
    ({ type }) => type === 'bench:diagnostic').data;
  assert.strictEqual(diagnostic.message, 'null');
  assert.strictEqual(records.at(-1).data.success, false);
}

for (const isolation of ['process', 'none']) {
  const result = spawnBench([
    `--bench-isolation=${isolation}`,
    '--bench-reporter=json',
    fixtures.path('bench-runner/load-error-after-declaration.cjs'),
  ]);
  assert.strictEqual(result.status, 1);
  const records = parseRecords(result);
  assert(records.some(({ type, data }) =>
    type === 'bench:diagnostic' &&
    data.message === 'load failed after declaration'));
  const plan = records.find(({ type }) => type === 'bench:plan').data;
  const completion = records.find(
    ({ type }) => type === 'bench:complete').data;
  assert.strictEqual(plan.name, 'declared before load error');
  assert.strictEqual(plan.selected, true);
  assert.strictEqual(completion.name, plan.name);
  assert.strictEqual(completion.error, undefined);
  assert.strictEqual(completion.samples.length, 1);
  assert.strictEqual(records.at(-1).data.counts.completed, 1);
}

{
  const result = spawnBench([
    '--bench-reporter=json',
    fixtures.path('bench-runner/recorded-detail.cjs'),
  ]);
  assert.strictEqual(result.status, 0);
  const records = parseRecords(result);
  const completion = records.find(
    ({ type }) => type === 'bench:complete').data;
  assert.strictEqual(completion.samples.length, 1);
  const { rate, ...sample } = completion.samples[0];
  assert.deepStrictEqual(sample, {
    detail: { index: 0, phase: 'measurement', value: '42' },
    duration_ns: '4',
    operations: 2,
  });
  assert(Math.abs(rate - 500_000_000) < 1);
}

{
  const result = spawnBench([
    '--bench-reporter=json',
    fixtures.path('bench-runner/diagnostic.cjs'),
  ]);
  assert.strictEqual(result.status, 0);
  const records = parseRecords(result);
  const diagnostic = records.find(
    ({ type }) => type === 'bench:diagnostic').data;
  const completion = records.find(
    ({ type }) => type === 'bench:complete').data;
  assert.deepStrictEqual(diagnostic.message, {
    name: 'node:bench:test:diagnostic',
    message: { value: '42' },
  });
  assert.strictEqual(diagnostic.level, 'info');
  assert.strictEqual(diagnostic.phase, 'measurement');
  assert.strictEqual(diagnostic.index, 0);
  assert.strictEqual(diagnostic.detail, undefined);
  assert.strictEqual(diagnostic.benchId, completion.benchId);
  assert.strictEqual(diagnostic.fileRunId, completion.fileRunId);
  assert.strictEqual(completion.error, undefined);
}

for (const { kind, message } of [
  { kind: 'sequence', message: /valid record sequence/ },
  { kind: 'record', message: /not a valid benchmark record/ },
  { kind: 'identity', message: /not a valid benchmark record/ },
  { kind: 'name-path', message: /not a valid benchmark record/ },
  { kind: 'plan', message: /not a valid benchmark plan/ },
  { kind: 'timeout', message: /not a valid benchmark plan/ },
  { kind: 'diagnostic', message: /not a valid benchmark diagnostic/ },
  { kind: 'diagnostic-order', message: /valid lifecycle sequence/ },
  { kind: 'summary', message: /not a valid benchmark summary/ },
]) {
  const result = spawnBench([
    '--bench-reporter=json',
    fixtures.path('bench-runner/malformed-record.cjs'),
  ], {
    env: {
      __proto__: null,
      ...process.env,
      NODE_BENCH_MALFORMED_RECORD: kind,
    },
  });
  assert.strictEqual(result.status, 1);
  const records = parseRecords(result);
  const diagnostics = records.filter(
    ({ type }) => type === 'bench:diagnostic');
  assert(diagnostics.some(({ data }) => message.test(data.message)));
  assert.strictEqual(records.at(-1).data.success, false);
}

{
  const result = spawnBench([
    '--bench-reporter=json',
    fixtures.path('bench-runner/malformed-plan-order.mjs'),
  ]);
  assert.strictEqual(result.status, 1);
  const records = parseRecords(result);
  const diagnostics = records.filter(
    ({ type }) => type === 'bench:diagnostic');
  assert(diagnostics.some(
    ({ data }) => /valid lifecycle sequence/.test(data.message)));
  assert.strictEqual(records.at(-1).data.success, false);
}

{
  const result = spawnBench([
    '--bench-isolation=none',
    '--bench-reporter=json',
    fixtures.path('bench-runner/a.cjs'),
    fixtures.path('bench-runner/exit-code.cjs'),
  ]);
  assert.strictEqual(result.status, 1);
  const records = parseRecords(result);
  const diagnostic = records.find(({ type, data }) =>
    type === 'bench:diagnostic' && /set exit code/.test(data.message)).data;
  assert.strictEqual(diagnostic.entryFile, null);
  assert.strictEqual(diagnostic.fileRunId, null);
  assert.strictEqual(diagnostic.file, null);
}

for (const { mode, message } of [
  { mode: 'code', message: /failed with exit code 2/ },
  { mode: 'late', message: /failed with exit code 2/ },
  ...common.isWindows ? [] : [
    { mode: 'signal', message: /failed with signal SIGTERM/ },
  ],
]) {
  const result = spawnBench([
    '--bench-reporter=json',
    fixtures.path('bench-runner/abrupt-exit.cjs'),
  ], {
    env: {
      __proto__: null,
      ...process.env,
      NODE_BENCH_EXIT_MODE: mode,
    },
  });
  assert.strictEqual(result.status, 1);
  const records = parseRecords(result);
  const diagnostics = records.filter(
    ({ type }) => type === 'bench:diagnostic');
  assert(diagnostics.some(({ data }) => message.test(data.message)));
}

for (const mode of ['callback', 'throw']) {
  const result = spawnBench([
    '--bench-reporter=json',
    fixtures.path('bench-runner/send-error.cjs'),
  ], {
    env: {
      __proto__: null,
      ...process.env,
      NODE_BENCH_SEND_ERROR: mode,
    },
  });
  assert.strictEqual(result.status, 1);
  const records = parseRecords(result);
  const diagnostics = records.filter(
    ({ type }) => type === 'bench:diagnostic');
  const messages = diagnostics.map(({ data }) => data.message).join('');
  assert.match(messages, /benchmark send/);
}

{
  const result = spawnBench([
    '--bench-reporter=json',
    fixtures.path('bench-runner/run.cjs'),
  ]);
  assert.strictEqual(result.status, 1);
  const records = parseRecords(result);
  const diagnostic = records.find(
    ({ type }) => type === 'bench:diagnostic');
  assert.match(diagnostic.data.message,
               /run\(\) cannot be called from a file run with --bench/);
  assert.strictEqual(records.at(-1).data.success, false);
}

{
  const result = spawnBench([
    '--bench-reporter=json',
    fixtures.path('bench-runner/exit-code.cjs'),
  ]);
  assert.strictEqual(result.status, 1);
  const records = parseRecords(result);
  assert(records.some(({ type, data }) =>
    type === 'bench:diagnostic' && /set exit code/.test(data.message)));
  assert.strictEqual(records.at(-1).data.success, false);
}
