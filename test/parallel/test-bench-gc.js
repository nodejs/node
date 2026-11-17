// Flags: --expose-gc --experimental-bench --no-warnings
'use strict';

const common = require('../common');
const assert = require('assert');
const { spawnSync } = require('child_process');
const fixtures = require('../common/fixtures');
const { bench, createRunner } = require('node:bench');

const spawnTimeout = common.platformTimeout(30_000);
const fixture = fixtures.path('bench-runner/gc.cjs');

function spawnNode(args, env = undefined) {
  const result = spawnSync(process.execPath, args, {
    __proto__: null,
    encoding: 'utf8',
    env: { ...process.env, ...env },
    timeout: spawnTimeout,
  });
  assert.ifError(result.error);
  assert.strictEqual(result.signal, null);
  return result;
}

function parseRecords(output) {
  return output.trim().split('\n').map((line) => JSON.parse(line));
}

function countForcedCollections(output) {
  return output.split('\n').filter((line) => /^\[\d+:.*testing/.test(line))
    .length;
}

// Invalid modes are rejected when declared or configured.
for (const gc of ['always', true, null]) {
  assert.throws(() => bench('invalid', { gc }, () => {}),
                { code: 'ERR_INVALID_ARG_VALUE' });
  assert.throws(() => createRunner({ gc }),
                { code: 'ERR_INVALID_ARG_VALUE' });
  assert.throws(() => createRunner().run({ gc }),
                { code: 'ERR_INVALID_ARG_VALUE' });
}

(async () => {
  // The effective mode is reported in the plan. run() overrides
  // createRunner(), which overrides the benchmark option.
  for (const { runner, run: runGc, expected } of [
    { expected: ['none', 'once', 'sample'] },
    { runner: 'sample', expected: ['sample', 'sample', 'sample'] },
    { runner: 'sample', run: 'none', expected: ['none', 'none', 'none'] },
  ]) {
    const instance = createRunner({ gc: runner, yieldBetweenSamples: false });
    for (const gc of [undefined, 'once', 'sample']) {
      instance.bench(`gc ${gc}`, { gc, samples: 2, warmup: 1 }, (b) => {
        b.record({ duration_ns: 1n });
      });
    }
    const records = await instance.run({ gc: runGc }).toArray();
    const plans = records.filter(({ type }) => type === 'bench:plan');
    assert.deepStrictEqual(plans.map(({ data }) => data.gc), expected);
    const completions = records.filter(({ type }) => type === 'bench:complete');
    assert.deepStrictEqual(
      completions.map(({ data }) => [data.error, data.samples.length]),
      [[undefined, 2], [undefined, 2], [undefined, 2]]);
  }

  // A forced collection runs once before the first measured sample for
  // 'once', and before every measured sample for 'sample'.
  for (const { args = [], env, expected } of [
    { expected: 0 },
    { env: { BENCH_GC_OPTION: 'once' }, expected: 1 },
    { env: { BENCH_GC_OPTION: 'sample' }, expected: 3 },
    { args: ['--bench-gc=once'], expected: 1 },
    { args: ['--bench-gc=none'], env: { BENCH_GC_OPTION: 'sample' },
      expected: 0 },
  ]) {
    const result = spawnNode([
      '--expose-gc', '--trace-gc', '--no-warnings', '--experimental-bench',
      '--bench', '--bench-isolation=none', '--bench-reporter=json',
      '--bench-reporter-destination=stderr', ...args, fixture,
    ], env);
    assert.strictEqual(result.status, 0, result.stderr);
    assert.strictEqual(countForcedCollections(result.stdout), expected);
  }

  // Without --expose-gc, benchmarks that request collection fail.
  {
    const result = spawnNode([
      '--no-warnings', '--experimental-bench', '--bench',
      '--bench-isolation=none', '--bench-reporter=json',
      '--bench-reporter-destination=stdout', fixture,
    ], { BENCH_GC_OPTION: 'once' });
    assert.strictEqual(result.status, 1);
    const complete = parseRecords(result.stdout)
      .find(({ type }) => type === 'bench:complete');
    assert.strictEqual(complete.data.error.code, 'ERR_INVALID_STATE');
    assert.match(complete.data.error.message,
                 /'once' gc mode requires the --expose-gc flag/);
  }

  // With process isolation, --bench-gc exposes gc to the child process.
  {
    const result = spawnNode([
      '--no-warnings', '--experimental-bench', '--bench', '--bench-gc=sample',
      '--bench-reporter=json', '--bench-reporter-destination=stdout', fixture,
    ]);
    assert.strictEqual(result.status, 0, result.stderr);
    const records = parseRecords(result.stdout);
    const plan = records.find(({ type }) => type === 'bench:plan');
    assert.strictEqual(plan.data.gc, 'sample');
    const complete = records.find(({ type }) => type === 'bench:complete');
    assert.strictEqual(complete.data.error, undefined);
    assert.deepStrictEqual(
      complete.data.samples.map(({ detail }) => detail),
      ['function', 'function', 'function']);
  }

  // Invalid --bench-gc values are rejected at startup.
  {
    const result = spawnNode([
      '--experimental-bench', '--bench', '--bench-gc=always', fixture,
    ]);
    assert.strictEqual(result.status, 9);
    assert.match(result.stderr, /invalid value for --bench-gc/);
  }
})().then(common.mustCall());
