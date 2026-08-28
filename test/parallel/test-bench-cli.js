'use strict';

const common = require('../common');
const assert = require('assert');
const { spawnSync } = require('child_process');
const fs = require('fs');
const fixtures = require('../common/fixtures');
const tmpdir = require('../common/tmpdir');

const basicPattern = fixtures.path('bench-runner/[ab].*');
const spawnTimeout = common.platformTimeout(30_000);

tmpdir.refresh();

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
  return spawnNode(['--no-warnings', '--bench', ...args], options);
}

function parseRecords(result) {
  assert.strictEqual(result.stderr, '');
  return parseOutput(result.stdout);
}

function parseOutput(output) {
  return output.trim().split('\n').map((line) => JSON.parse(line));
}

{
  const result = spawnBench([]);
  assert.strictEqual(result.status, 1);
  assert.strictEqual(result.stdout, '');
  assert.match(result.stderr, /--bench requires at least one file or glob/);
}

{
  const result = spawnBench(['does-not-exist-*.js']);
  assert.strictEqual(result.status, 1);
  assert.strictEqual(result.stdout, '');
  assert.match(result.stderr, /^Could not find/);
}

if (common.canCreateSymLink()) {
  const dangling = tmpdir.resolve('dangling.cjs');
  fs.symlinkSync(tmpdir.resolve('missing.cjs'), dangling);
  const result = spawnBench([dangling]);
  assert.strictEqual(result.status, 1);
  assert.strictEqual(result.stdout, '');
  assert.match(result.stderr, /^Could not find/);
}

for (const { patterns, message } of [
  {
    patterns: [
      fixtures.path('bench-runner/a.cjs'),
      fixtures.path('bench-runner/b.mjs'),
    ],
    message: /benchmark child process requires exactly one file/,
  },
  {
    patterns: [fixtures.path('bench-runner/missing.cjs')],
    message: /^Could not find/,
  },
]) {
  const result = spawnNode([
    '--no-warnings',
    '--require', fixtures.path('bench-runner/fake-ipc.cjs'),
    '--bench',
    ...patterns,
  ], {
    __proto__: null,
    env: { __proto__: null, ...process.env, NODE_BENCH_CONTEXT: 'child' },
  });
  assert.strictEqual(result.status, 1);
  assert.strictEqual(result.stdout, '');
  assert.match(result.stderr, message);
}

{
  const result = spawnBench(['--bench-reporter=json', basicPattern]);
  assert.strictEqual(result.status, 0);
  const records = parseRecords(result);
  const completions = records.filter(
    ({ type }) => type === 'bench:complete');

  assert.deepStrictEqual(completions.map(({ data }) => data.name), [
    'alpha',
    'beta',
  ]);
  assert.notStrictEqual(
    completions[0].data.params.pid,
    completions[1].data.params.pid,
  );
  assert.deepStrictEqual(completions.map(({ data }) => data.params.file), [
    'a',
    'b',
  ]);
  assert.match(completions[0].data.samples[0].duration_ns, /^\d+$/);
  assert(Number.isFinite(completions[0].data.summary.mean));

  const summaries = records.filter(
    ({ type }) => type === 'bench:summary');
  assert.strictEqual(summaries.length, 1);
  assert.strictEqual(summaries[0].data.file, null);
  assert.deepStrictEqual(summaries[0].data.counts, {
    completed: 2,
    failed: 0,
    skipped: 0,
    total: 2,
  });
}

{
  const result = spawnBench([
    '--bench-reporter=json',
    '--bench-isolation=none',
    basicPattern,
  ]);
  assert.strictEqual(result.status, 0);
  const records = parseRecords(result);
  const completions = records.filter(
    ({ type }) => type === 'bench:complete');
  assert.strictEqual(completions.length, 2);
  assert.strictEqual(
    completions[0].data.params.pid,
    completions[1].data.params.pid,
  );
  assert.strictEqual(records.at(-1).data.file, null);
}

{
  const result = spawnBench([
    '--bench-reporter=json',
    '--bench-name-pattern=selected',
    '--bench-samples=2',
    '--bench-warmup=3',
    fixtures.path('bench-runner/options.cjs'),
  ]);
  assert.strictEqual(result.status, 0);
  const records = parseRecords(result);
  const samples = records.filter(({ type }) => type === 'bench:sample');
  assert.deepStrictEqual(samples.map(({ data }) => data.operations), [4, 5]);

  const completions = records.filter(
    ({ type }) => type === 'bench:complete');
  const selected = completions.find(({ data }) => data.name === 'selected').data;
  assert.deepStrictEqual(selected.params, {
    boolean: true,
    number: 42,
    string: 'value',
  });
  assert.strictEqual(selected.samples.length, 2);
  assert.strictEqual(
    selected.summary.mean,
    (selected.samples[0].rate + selected.samples[1].rate) / 2,
  );
  assert(Number.isFinite(selected.summary.medianConfidenceInterval.lower));

  const filtered = completions.find(
    ({ data }) => data.name === 'filtered out').data;
  assert.strictEqual(filtered.skip, 'name pattern');
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

for (const { file, status } of [
  { file: 'a.cjs', status: 0 },
  { file: 'error.cjs', status: 1 },
]) {
  const result = spawnBench([
    `--bench-reporter=${fixtures.fileURL('bench-runner/verifying-reporter.cjs')}`,
    fixtures.path(`bench-runner/${file}`),
  ]);
  assert.strictEqual(result.status, status);
  assert.strictEqual(result.stderr, '');
  assert.strictEqual(result.stdout, 'verified\n');
}

{
  const result = spawnBench([
    '--require',
    fixtures.path('bench-runner/preload.cjs'),
    '--bench-reporter=json',
    fixtures.path('bench-runner/preloaded.cjs'),
  ]);
  assert.strictEqual(result.status, 0);
  const records = parseRecords(result);
  const completion = records.find(
    ({ type }) => type === 'bench:complete');
  assert.deepStrictEqual(completion.data.params, { preloaded: true });
}

for (const isolation of ['process', 'none']) {
  const result = spawnBench([
    `--bench-isolation=${isolation}`,
    '--bench-reporter=json',
    '--import', fixtures.fileURL('bench-runner/import.mjs'),
    fixtures.path('bench-runner/imported.cjs'),
  ]);
  assert.strictEqual(result.status, 0);
  const records = parseRecords(result);
  const completion = records.find(
    ({ type }) => type === 'bench:complete');
  assert.deepStrictEqual(completion.data.params, { imported: 'loaded' });
}

{
  const lock = tmpdir.resolve('serial.lock');
  const result = spawnBench([
    '--bench-reporter=json',
    fixtures.path('bench-runner/serial-[ab].cjs'),
  ], {
    env: { __proto__: null, ...process.env, NODE_BENCH_LOCK: lock },
  });
  assert.strictEqual(result.status, 0);
  const records = parseRecords(result);
  assert.deepStrictEqual(records.filter(
    ({ type }) => type === 'bench:complete').map(({ data }) => data.name), [
    'serial a',
    'serial b',
  ]);
  assert.strictEqual(fs.existsSync(lock), false);
}

{
  const result = spawnBench([
    '--bench-reporter=json',
    fixtures.path('bench-runner/ipc.cjs'),
  ]);
  assert.strictEqual(result.status, 0);
  const records = parseRecords(result);
  assert.strictEqual(records.find(
    ({ type }) => type === 'bench:complete').data.name, 'user IPC');
  assert.strictEqual(records.at(-1).data.file,
                     fixtures.path('bench-runner/ipc.cjs'));
}

for (const { kind, message } of [
  { kind: 'record', message: /not a valid benchmark record/ },
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
    '--stack-trace-limit=17',
    '--random-seed=17',
    '--bench-reporter=json',
    fixtures.path('bench-runner/v8-option.cjs'),
  ]);
  assert.strictEqual(result.status, 0);
  const records = parseRecords(result);
  assert.strictEqual(records.find(
    ({ type }) => type === 'bench:complete').data.name, 'V8 option');
}

if (common.hasInspector) {
  const result = spawnBench([
    '--inspect=0',
    '--bench-reporter=json',
    fixtures.path('bench-runner/inspector.cjs'),
  ]);
  assert.strictEqual(result.status, 0);
  const records = parseRecords(result);
  const diagnostics = records.filter(
    ({ type }) => type === 'bench:diagnostic')
    .map(({ data }) => data.message).join('');
  assert.match(diagnostics, /Debugger listening on ws:\/\//);
  const completion = records.find(
    ({ type }) => type === 'bench:complete').data;
  assert.strictEqual(completion.params.inspectPort, '--inspect-port=0');
}

{
  const result = spawnBench([
    '--bench-reporter=json',
    '--bench-reporter-destination=stderr',
    fixtures.path('bench-runner/output.cjs'),
  ]);
  assert.strictEqual(result.status, 0);
  assert.strictEqual(result.stdout, '');
  const records = parseOutput(result.stderr);
  const diagnostics = records.filter(
    ({ type }) => type === 'bench:diagnostic');
  assert.deepStrictEqual(diagnostics.map(({ data }) => data.stream).sort(), [
    'stderr',
    'stdout',
  ]);
  assert(diagnostics.some(
    ({ data }) => data.message === 'benchmark stdout\n'));
  assert(diagnostics.some(
    ({ data }) => data.message === 'benchmark stderr\n'));
}

{
  const result = spawnBench([
    '--bench-reporter=json',
    fixtures.path('bench-runner/utf8-output.cjs'),
  ]);
  assert.strictEqual(result.status, 0);
  const records = parseRecords(result);
  const output = records.filter(
    ({ type, data }) => type === 'bench:diagnostic' &&
      data.stream === 'stdout').map(({ data }) => data.message).join('');
  assert.strictEqual(output, 'split:\u20ac\n');
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
    `--bench-reporter=${fixtures.fileURL('bench-runner/failing-reporter.cjs')}`,
    fixtures.path('bench-runner/a.cjs'),
  ]);
  assert.strictEqual(result.status, 1);
  assert.match(result.stderr, /benchmark reporter failed/);
}

{
  const result = spawnBench([
    `--bench-reporter=${fixtures.fileURL('bench-runner/slow-reporter.cjs')}`,
    fixtures.path('bench-runner/many-records.cjs'),
  ]);
  assert.strictEqual(result.status, 0);
  assert.strictEqual(result.stderr, '');
  const report = JSON.parse(result.stdout);
  assert.strictEqual(report.samples, 30);
  assert.strictEqual(report.stdout,
                     Array.from({ length: 30 }, (_, i) => `${i}\n`).join(''));
}

{
  const result = spawnBench([
    `--bench-reporter=${fixtures.fileURL('bench-runner/destroying-reporter.cjs')}`,
    fixtures.path('bench-runner/a.cjs'),
  ]);
  assert.strictEqual(result.status, 1);
  assert.match(result.stderr, /benchmark reporter closed the stream/);
}

{
  const result = spawnBench([
    '--bench-reporter=json',
    '--bench-reporter-destination=stdout',
    '--bench-reporter=data:text/javascript,export default 0',
    '--bench-reporter-destination=stderr',
    fixtures.path('bench-runner/a.cjs'),
  ]);
  assert.strictEqual(result.status, 1);
  assert.match(result.stderr, /is not a valid reporter/);
}

{
  const result = spawnBench([
    '--bench-reporter=json',
    fixtures.path('bench-runner/exit-code.cjs'),
  ]);
  assert.strictEqual(result.status, 1);
  const records = parseRecords(result);
  assert.strictEqual(records.at(-1).data.success, false);
}

{
  const result = spawnNode([
    '--no-warnings',
    `--experimental-config-file=${fixtures.path('bench-runner/node.config.json')}`,
    fixtures.path('bench-runner/a.cjs'),
  ]);
  assert.strictEqual(result.status, 0);
  const records = parseRecords(result);
  assert.strictEqual(records.find(
    ({ type }) => type === 'bench:complete').data.samples.length, 1);
}

{
  const destination = tmpdir.resolve('bench.txt');
  const result = spawnBench([
    '--bench-reporter=json',
    '--bench-reporter-destination=stdout',
    '--bench-reporter=spec',
    `--bench-reporter-destination=${destination}`,
    fixtures.path('bench-runner/a.cjs'),
  ]);
  assert.strictEqual(result.status, 0);
  parseRecords(result);
  assert.match(fs.readFileSync(destination, 'utf8'),
               /^benchmark \| samples \| mean rate/);
}

{
  const result = spawnBench([fixtures.path('bench-runner/a.cjs')]);
  assert.strictEqual(result.status, 0);
  assert.strictEqual(result.stderr, '');
  assert.match(result.stdout, /^benchmark \| samples \| mean rate/);
  assert.match(result.stdout, /1 completed, 0 failed, 0 skipped/);
}

{
  const result = spawnBench([
    '--bench-reporter=json',
    '--bench-reporter=spec',
    fixtures.path('bench-runner/a.cjs'),
  ]);
  assert.strictEqual(result.status, 1);
  assert.strictEqual(result.stdout, '');
  assert.match(result.stderr,
               /must match the number of specified '.*destination'/);
}

for (const { args, message } of [
  {
    args: ['--bench-isolation=invalid', 'unused.js'],
    message: /invalid value for --bench-isolation/,
  },
  {
    args: ['--bench-samples=0', 'unused.js'],
    message: /--bench-samples must be greater than 0/,
  },
  {
    args: ['--bench-warmup=4294967296', 'unused.js'],
    message: /--bench-warmup.*out of range/,
  },
  {
    args: ['--bench-samples=2x', 'unused.js'],
    message: /invalid value for --bench-samples/,
  },
  {
    args: ['--bench-warmup=abc', 'unused.js'],
    message: /invalid value for --bench-warmup/,
  },
  {
    args: ['--bench-name-pattern=[', 'unused.js'],
    message: /invalid regular expression/,
  },
  {
    args: ['--eval=1', 'unused.js'],
    message: /either --bench or --eval can be used, not both/,
  },
  {
    args: ['--interactive', 'unused.js'],
    message: /either --bench or --interactive can be used, not both/,
  },
  {
    args: ['--watch', 'unused.js'],
    message: /either --bench or --watch can be used, not both/,
  },
  {
    args: ['--watch-path=.', 'unused.js'],
    message: /either --bench or --watch can be used, not both/,
  },
  {
    args: ['--check', 'unused.js'],
    message: /either --bench or --check can be used, not both/,
  },
  {
    args: ['--test', 'unused.js'],
    message: /either --bench or --test can be used, not both/,
  },
]) {
  const result = spawnBench(args);
  assert.notStrictEqual(result.status, 0);
  assert.match(result.stderr, message);
}
