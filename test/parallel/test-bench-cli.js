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
  const plans = records.filter(
    ({ type }) => type === 'bench:plan').map(({ data }) => data);
  const completions = records.filter(
    ({ type }) => type === 'bench:complete');

  assert.deepStrictEqual(plans.map(({ name }) => name), ['alpha', 'beta']);
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

for (const isolation of ['process', 'none']) {
  const result = spawnBench([
    `--bench-isolation=${isolation}`,
    '--bench-reporter=json',
    fixtures.path('bench-runner/identity-entry-*.cjs'),
  ]);
  assert.strictEqual(result.status, 0);
  const records = parseRecords(result);
  const plans = records.filter(
    ({ type }) => type === 'bench:plan').map(({ data }) => data);
  const completions = records.filter(
    ({ type }) => type === 'bench:complete').map(({ data }) => data);
  const summary = records.at(-1).data;

  assert.strictEqual(plans.length, 2);
  for (const plan of plans) {
    const planIndex = records.findIndex(({ type, data }) =>
      type === 'bench:plan' && data.fileRunId === plan.fileRunId);
    const startIndex = records.findIndex(({ type, data }) =>
      type === 'bench:start' && data.fileRunId === plan.fileRunId);
    assert(planIndex < startIndex);
  }
  assert.strictEqual(completions.length, 2);
  assert.strictEqual(completions[0].benchId, completions[1].benchId);
  assert.strictEqual(completions[0].runId, completions[1].runId);
  assert.strictEqual(completions[0].runId, summary.runId);
  assert.notStrictEqual(
    completions[0].fileRunId, completions[1].fileRunId);
  assert.deepStrictEqual(completions.map(({ entryFile }) => entryFile), [
    fixtures.path('bench-runner/identity-entry-a.cjs'),
    fixtures.path('bench-runner/identity-entry-b.cjs'),
  ]);
  assert.deepStrictEqual(completions.map(({ namePath }) => namePath), [
    ['shared suite', 'shared identity'],
    ['shared suite', 'shared identity'],
  ]);
  assert.strictEqual(summary.fileRunId, null);
  assert.strictEqual(summary.entryFile, null);
}

{
  const result = spawnBench([
    '--bench-reporter=json',
    fixtures.path('bench-runner/identity-suite.cjs'),
  ]);
  assert.strictEqual(result.status, 0);
  const records = parseRecords(result);
  const completions = records.filter(
    ({ type }) => type === 'bench:complete').map(({ data }) => data);
  const parentId = JSON.stringify([
    fixtures.path('bench-runner/identity-suite.cjs'),
    ['cross-module suite'],
  ]);

  assert.deepStrictEqual(completions.map(({ name }) => name), [
    'child a',
    'child b',
  ]);
  assert(completions.every((completion) =>
    completion.parentId === parentId));
  assert.deepStrictEqual(completions.map(({ namePath }) => namePath), [
    ['cross-module suite', 'child a'],
    ['cross-module suite', 'child b'],
  ]);
  assert(completions.every(({ entryFile }) =>
    entryFile === fixtures.path('bench-runner/identity-suite.cjs')));
}

for (const isolation of ['process', 'none']) {
  const result = spawnBench([
    '--require', fixtures.path('bench-runner/identity-preload.cjs'),
    `--bench-isolation=${isolation}`,
    '--bench-reporter=json',
    fixtures.path('bench-runner/identity-entry-*.cjs'),
  ]);
  assert.strictEqual(result.status, 0);
  const records = parseRecords(result);
  const preloads = records.filter(
    ({ type, data }) => type === 'bench:complete' &&
      data.name === 'preload identity').map(({ data }) => data);
  assert.strictEqual(preloads.length, isolation === 'process' ? 2 : 1);
  assert.strictEqual(
    new Set(preloads.map(({ fileRunId }) => fileRunId)).size,
    preloads.length,
  );
  assert(preloads.every(({ entryFile }) => entryFile === null));
  assert(preloads.every(
    ({ runId }) => runId === records.at(-1).data.runId));
}

{
  const result = spawnBench([
    '--bench-isolation=none',
    '--bench-reporter=json',
    fixtures.path('bench-runner/a.cjs'),
    fixtures.path('bench-runner/identity-hook.cjs'),
  ]);
  assert.strictEqual(result.status, 1);
  const records = parseRecords(result);
  const diagnostic = records.find(
    ({ type, data }) => type === 'bench:diagnostic' &&
      data.message === 'scoped hook failed').data;
  const completion = records.find(
    ({ type, data }) => type === 'bench:complete' &&
      data.name === 'scoped hook benchmark').data;
  assert.strictEqual(diagnostic.entryFile,
                     fixtures.path('bench-runner/identity-hook.cjs'));
  assert.strictEqual(diagnostic.fileRunId, completion.fileRunId);
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
  const plans = records.filter(
    ({ type }) => type === 'bench:plan').map(({ data }) => data);
  const samples = records.filter(({ type }) => type === 'bench:sample');
  assert.deepStrictEqual(samples.map(({ data }) => data.operations), [4, 5]);
  assert.deepStrictEqual(plans.map((plan) => ({
    name: plan.name,
    samples: plan.samples,
    selected: plan.selected,
    skip: plan.skip,
    timeout: plan.timeout,
    warmup: plan.warmup,
    yieldBetweenSamples: plan.yieldBetweenSamples,
  })), [
    {
      name: 'selected',
      samples: 2,
      selected: true,
      skip: undefined,
      timeout: null,
      warmup: 3,
      yieldBetweenSamples: true,
    },
    {
      name: 'filtered out',
      samples: 2,
      selected: false,
      skip: 'name pattern',
      timeout: null,
      warmup: 3,
      yieldBetweenSamples: true,
    },
  ]);

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

{
  const result = spawnBench([
    '--bench-reporter=json',
    '--random-seed=17',
    '--stack-trace-limit=17',
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
