// Flags: --experimental-bench --no-warnings
'use strict';

const common = require('../common');
const { completeSample } = require('../common/bench');
const assert = require('assert');
const os = require('os');
const { Readable } = require('stream');
const { bench, run } = require('node:bench');
const { json, spec } = require('node:bench/reporters');

bench('json completed', {
  params: { size: 'small' },
  samples: 1,
}, (b) => {
  completeSample(b);
});

bench('json failed', { samples: 1 }, () => {
  const error = new Error('benchmark failed', { cause: 7n });
  error.code = 'ERR_BENCHMARK_EXAMPLE';
  throw error;
});

(async () => {
  const chunks = await run().compose(json).toArray();
  const lines = chunks.join('').trim().split('\n');
  const records = lines.map((line) => JSON.parse(line));

  assert.strictEqual(records.length, 9);
  const { type: environmentType, data: environment } = records[0];
  assert.strictEqual(environmentType, 'bench:environment');
  assert.strictEqual(environment.node, process.version);
  assert.strictEqual(environment.v8, process.versions.v8);
  assert.strictEqual(environment.platform, process.platform);
  assert.strictEqual(environment.arch, process.arch);
  assert.strictEqual(environment.os.type, os.type());
  assert.strictEqual(environment.os.release, os.release());
  assert.strictEqual(environment.os.machine, os.machine());
  assert.strictEqual(
    environment.cpus.reduce((total, { count }) => total + count, 0),
    os.cpus().length);
  assert.strictEqual(environment.availableParallelism,
                     os.availableParallelism());
  assert.strictEqual(environment.memory.total, os.totalmem());
  assert.strictEqual(typeof environment.memory.free, 'number');
  assert.strictEqual(environment.loadavg.length, 3);
  assert.deepStrictEqual(environment.execArgv, process.execArgv);
  const plans = records.filter(({ type }) => type === 'bench:plan');
  assert.deepStrictEqual(plans.map(({ data }) => data.selected), [true, true]);
  const sample = records.find(({ type }) => type === 'bench:sample');
  assert.match(sample.data.duration_ns, /^\d+$/);

  const completions = records.filter(
    ({ type }) => type === 'bench:complete');
  const completed = completions.find(
    ({ data }) => data.name === 'json completed').data;
  assert.match(completed.samples[0].duration_ns, /^\d+$/);

  const failed = completions.find(
    ({ data }) => data.name === 'json failed').data;
  assert.strictEqual(failed.error.name, 'Error');
  assert.strictEqual(failed.error.message, 'benchmark failed');
  assert.strictEqual(failed.error.code, 'ERR_BENCHMARK_EXAMPLE');
  assert.strictEqual(failed.error.cause, '7');
  assert.match(failed.error.stack, /benchmark failed/);

  const summary = records.find(
    ({ type }) => type === 'bench:summary').data;
  assert.match(summary.duration_ns, /^\d+$/);
  assert.deepStrictEqual(summary.counts, {
    completed: 1,
    failed: 1,
    skipped: 0,
    total: 2,
  });
  // The failed run sets the exit code; reset it so the test itself passes.
  assert.strictEqual(process.exitCode, 1);
  process.exitCode = 0;

  const synthetic = [
    {
      type: 'bench:complete',
      data: {
        name: 'fast',
        params: { size: 'small' },
        samples: [{}, {}],
        summary: {
          mean: 1500,
          median: 1400,
          coefficientOfVariation: 0.1,
          confidenceInterval: { lower: 1000, upper: 2000 },
          skewness: 2,
        },
      },
    },
    {
      type: 'bench:complete',
      data: {
        name: 'later',
        params: {},
        samples: [],
        skip: 'not supported',
      },
    },
    {
      type: 'bench:complete',
      data: {
        name: 'broken',
        params: {},
        samples: [],
        error: new Error('boom'),
      },
    },
    {
      type: 'bench:diagnostic',
      data: { message: 'suite problem' },
    },
    {
      type: 'bench:summary',
      data: {
        counts: { completed: 1, failed: 1, skipped: 1, total: 3 },
      },
    },
  ];
  const specChunks = await Readable.from(synthetic).compose(spec).toArray();
  const specOutput = specChunks.join('');
  const specHeaderEnd = specOutput.indexOf('\n\n') + 2;
  const specHeader = specOutput.slice(0, specHeaderEnd);
  assert.match(specHeader,
               new RegExp(`^node: ${RegExp.escape(process.version)} ` +
                          `\\(V8 ${RegExp.escape(process.versions.v8)}\\), ` +
                          `${process.platform} ${process.arch} `));
  assert.match(specHeader, /^cpu: .*\(available parallelism: \d+\)$/m);
  assert.match(specHeader, /^memory: [\d.]+ \w+ total, [\d.]+ \w+ free/m);
  assert.match(specHeader, /^load average: [\d.]+, [\d.]+, [\d.]+$/m);
  assert.match(specHeader, /^exec argv: --experimental-bench --no-warnings$/m);
  assert.strictEqual(specOutput.slice(specHeaderEnd),
                     'benchmark | samples | mean rate | 95% CI | ' +
                     'median rate | warning\n' +
                     'fast [size="small"] | 2 | 1.50k ops/s | ' +
                     '[1.00k ops/s, 2.00k ops/s] | 1.40k ops/s | ' +
                     'noisy, skewed\n' +
                     'later | 0 | - | - | - | skipped: not supported\n' +
                     'broken | 0 | - | - | - | error: boom\n' +
                     'diagnostic: suite problem\n\n' +
                     '1 completed, 1 failed, 1 skipped\n');

  const circular = {};
  circular.self = circular;
  const aggregate = new AggregateError([1n], 'aggregate failure');
  const jsonEdgeChunks = await Readable.from([{
    type: 'bench:diagnostic',
    data: { aggregate, circular },
  }]).compose(json).toArray();
  assert.strictEqual(jsonEdgeChunks.length, 2);
  const jsonEdge = JSON.parse(jsonEdgeChunks[1]);
  assert.deepStrictEqual(jsonEdge.data.aggregate.errors, ['1']);
  assert.strictEqual(jsonEdge.data.circular.self, '[Circular]');

  async function* undefinedRecord() {
    yield undefined;
  }
  const undefinedChunks = [];
  for await (const chunk of json(undefinedRecord())) {
    undefinedChunks.push(chunk);
  }
  assert.strictEqual(undefinedChunks.length, 2);
  assert.strictEqual(JSON.parse(undefinedChunks[0]).type,
                     'bench:environment');
  assert.strictEqual(undefinedChunks[1], 'null\n');

  function result(name, rate) {
    return {
      type: 'bench:complete',
      data: {
        name,
        params: {},
        samples: [{}],
        summary: {
          mean: rate,
          median: rate,
          coefficientOfVariation: 0,
          confidenceInterval: { lower: rate, upper: rate },
          skewness: 0,
        },
      },
    };
  }

  const specEdgeChunks = await Readable.from([
    result('giga', 1_500_000_000),
    result('mega', 1_500_000),
    result('fractional', 0.5),
    {
      type: 'bench:complete',
      data: { name: 'skip', params: {}, samples: [], skip: true },
    },
    {
      type: 'bench:complete',
      data: { name: 'error', params: {}, samples: [], error: 'failure' },
    },
  ]).compose(spec).toArray();
  const specEdgeOutput = specEdgeChunks.join('');
  assert.match(specEdgeOutput, /giga \| 1 \| 1\.50G ops\/s/);
  assert.match(specEdgeOutput, /mega \| 1 \| 1\.50M ops\/s/);
  assert.match(specEdgeOutput, /fractional \| 1 \| 0\.500 ops\/s/);
  assert.match(specEdgeOutput, /skip \| 0 \| - \| - \| - \| skipped\n/);
  assert.match(specEdgeOutput,
               /error \| 0 \| - \| - \| - \| error: failure/);

  const emptySpecChunks = await Readable.from([]).compose(spec).toArray();
  assert.deepStrictEqual(emptySpecChunks, []);
})().then(common.mustCall());
