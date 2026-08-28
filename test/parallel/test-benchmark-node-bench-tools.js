// Flags: --no-warnings
'use strict';

require('../common');
const assert = require('assert');
const { spawnSync } = require('child_process');
const fs = require('fs');
const path = require('path');
const fixtures = require('../common/fixtures');
const tmpdir = require('../common/tmpdir');
const {
  analyzeScatter,
  holmAdjust,
  isRegressionFailure,
} = require('../../benchmark/_node-bench-analysis.js');
const { csvEncode } = require('../../benchmark/_node-bench.js');

const compare = path.resolve(__dirname, '../../benchmark/compare-node-bench.js');
const legacyScatter = path.resolve(__dirname, '../../benchmark/scatter.js');
const scatter = path.resolve(__dirname, '../../benchmark/scatter-node-bench.js');
const benchmark = fixtures.path('bench-runner/tools.cjs');

tmpdir.refresh();

assert.strictEqual(csvEncode(true), 'true');
assert.deepStrictEqual(holmAdjust([0.01, 0.03, 0.04]), [0.03, 0.06, 0.06]);
assert.strictEqual(isRegressionFailure({
  ci95: 3,
  improvement: -12,
  pAdjusted: 0.01,
}, 10), false);
assert.strictEqual(isRegressionFailure({
  ci95: 1,
  improvement: -12,
  pAdjusted: 0.06,
}, 10), false);
assert.strictEqual(isRegressionFailure({
  ci95: 1,
  improvement: -12,
  pAdjusted: 0.01,
}, 10), true);
assert.throws(
  () => analyzeScatter([{
    observation: 0,
    params: { size: 1 },
    rate: 1,
  }], 'size', 'size', false),
  /must name different parameters/,
);
assert.doesNotMatch(analyzeScatter([0, 1].map((observation) => ({
  observation,
  params: { size: 1 },
  rate: 1_234_567.89,
})), 'size', undefined, false), /\(!\)/);
assert.match(analyzeScatter([
  { observation: 0, params: { method: 'a', size: 1 }, rate: 10 },
  { observation: 0, params: { method: 'b', size: 1 }, rate: 20 },
  { observation: 1, params: { method: 'a', size: 1 }, rate: 30 },
  { observation: 1, params: { method: 'b', size: 1 }, rate: 50 },
], 'size', undefined, false), /\n\s*1\s+2\s+/);

function run(script, args, options = undefined) {
  return spawnSync(process.execPath, [script, ...args], {
    encoding: 'utf8',
    timeout: 30_000,
    ...options,
  });
}

{
  const pidLog = tmpdir.resolve('pids');
  const result = run(compare, [
    '--old', process.execPath,
    '--new', process.execPath,
    '--runs', '2',
    '--', benchmark,
  ], {
    env: { __proto__: null, ...process.env, NODE_BENCH_PID_LOG: pidLog },
  });
  assert.strictEqual(result.status, 0, result.stderr);
  assert.strictEqual(result.stderr, '');
  const lines = result.stdout.trim().split('\n');
  assert.strictEqual(lines[0],
                     '"binary","filename","configuration","rate","time"');
  assert.strictEqual(lines.length, 9);
  assert.strictEqual(lines.filter((line) => line.startsWith('"old",')).length,
                     4);
  assert.strictEqual(lines.filter((line) => line.startsWith('"new",')).length,
                     4);
  assert.deepStrictEqual(lines.slice(1).map((line) => line.slice(0, 5)), [
    '"old"', '"old"', '"new"', '"new"',
    '"new"', '"new"', '"old"', '"old"',
  ]);
  assert(lines.slice(1).every(
    (line) => line.includes('"tools/simple.js"')));
  const pids = fs.readFileSync(pidLog, 'utf8').trim().split('\n');
  assert.strictEqual(new Set(pids).size, 4);
}

{
  const result = run(scatter, [
    '--node', process.execPath,
    '--runs', '2',
    '--', benchmark,
  ]);
  assert.strictEqual(result.status, 0, result.stderr);
  assert.strictEqual(result.stderr, '');
  const lines = result.stdout.trim().split('\n');
  assert.strictEqual(lines[0],
                     '"filename","method","size","rate","time"');
  assert.strictEqual(lines.length, 5);
  assert(lines.slice(1).every(
    (line) => line.startsWith('"tools/simple.js","loop",')));
}

{
  const result = run(scatter, [
    '--runs', '1',
    '--', fixtures.path('bench-runner/tools-no-params.cjs'),
  ]);
  assert.strictEqual(result.status, 0, result.stderr);
  const lines = result.stdout.trim().split('\n');
  assert.strictEqual(lines[0], '"filename","rate","time"');
  assert.strictEqual(lines.length, 2);
  assert.strictEqual(lines[1].split(',').length, 3);
}

{
  const result = run(scatter, [
    '--runs', '1',
    '--', fixtures.path('bench-runner/tools-reserved-param.cjs'),
  ]);
  assert.strictEqual(result.status, 1);
  assert.match(result.stderr, /parameter 'rate' is reserved/);
}

{
  const result = run(scatter, [
    '--runs', '1',
    '--', fixtures.path('bench-runner/tools-collision.cjs'),
  ]);
  assert.strictEqual(result.status, 1);
  assert.match(result.stderr, /requires one logical benchmark name per file/);
}

{
  const result = run(compare, [
    '--old', process.execPath,
    '--new', process.execPath,
    '--runs', '1',
    '--', fixtures.path('bench-runner/tools-collision.cjs'),
  ]);
  assert.strictEqual(result.status, 1);
  assert.match(result.stderr, /Distinct benchmarks would share the CSV group/);
}

{
  const result = run(scatter, [
    '--runs', '2',
    '--', fixtures.path('bench-runner/a.cjs'),
  ]);
  assert.strictEqual(result.status, 1);
  assert.match(result.stderr, /set of reported benchmarks changed between runs/);
}

{
  const result = run(scatter, [
    '--runs', '1',
    '--name-pattern', 'missing',
    '--', benchmark,
  ]);
  assert.strictEqual(result.status, 1);
  assert.match(result.stderr, /No benchmark samples were produced/);
}

{
  const result = run(scatter, [
    '--runs', 'invalid',
    '--', benchmark,
  ]);
  assert.strictEqual(result.status, 1);
  assert.match(result.stderr, /--runs must be an integer/);
}

{
  const result = run(scatter, [
    '--runs', '2',
    '--analyze',
    '--xaxis', 'size',
    '--category', 'method',
    '--no-chart',
    '--', benchmark,
  ]);
  assert.strictEqual(result.status, 0, result.stderr);
  assert.strictEqual(result.stderr, '');
  assert.match(result.stdout,
               /size\s+method\s+samples\s+rate\s+confidence\.interval/);
  assert.match(result.stdout, /Change between consecutive size values/);
  assert.match(result.stdout, /Mann-Whitney U.*Cliff's delta/);
  assert.doesNotMatch(result.stdout, /"filename","method"/);
}

{
  const result = run(scatter, [
    '--analyze',
    '--', benchmark,
  ]);
  assert.strictEqual(result.status, 1);
  assert.match(result.stderr, /--analyze requires --xaxis/);
}

{
  const result = run(compare, [
    '--old', process.execPath,
    '--new', process.execPath,
    '--runs', '2',
    '--max-regression', '100',
    '--', fixtures.path('bench-runner/tools-no-params.cjs'),
  ]);
  assert.strictEqual(result.status, 0, result.stderr);
  assert.strictEqual(result.stderr, '');
  assert.match(result.stdout, /confidence\s+improvement\s+accuracy/);
  assert.match(result.stdout, /Holm-Bonferroni correction/);
  assert.match(result.stdout, /--max-regression uses the corrected values/);
  assert.doesNotMatch(result.stdout, /"binary","filename"/);
}

{
  const benchmark = path.resolve(
    __dirname, '../../benchmark/buffers/buffer-compare-offset.js');
  const nodeBenchmark = path.resolve(
    __dirname, '../../benchmark/buffers/_buffer-compare-offset.node-bench.js');
  const legacy = run(legacyScatter, [
    '--runs', '1',
    benchmark,
  ]);
  const modern = run(scatter, [
    '--runs', '1',
    '--', nodeBenchmark,
  ]);
  assert.strictEqual(legacy.status, 0, legacy.stderr);
  assert.strictEqual(modern.status, 0, modern.stderr);
  const legacyLines = legacy.stdout.trim().split('\n');
  const modernLines = modern.stdout.trim().split('\n');
  assert.deepStrictEqual(
    legacyLines[0].replaceAll(' ', '').split(',').sort(),
    modernLines[0].split(',').sort(),
  );
  assert.strictEqual(modernLines[0],
                     '"filename","method","n","size","rate","time"');
  assert.strictEqual(legacyLines.length, 9);
  assert.strictEqual(modernLines.length, 9);
  const name = path.join('buffers', 'buffer-compare-offset.js');
  assert(legacyLines[1].startsWith(`"${name}",`));
  assert(modernLines[1].startsWith(`"${name}",`));
}
