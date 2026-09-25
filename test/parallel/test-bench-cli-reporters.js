'use strict';

const common = require('../common');
const assert = require('assert');
const { spawnSync } = require('child_process');
const fs = require('fs');
const fixtures = require('../common/fixtures');
const tmpdir = require('../common/tmpdir');

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
    `--bench-reporter=${fixtures.fileURL('bench-runner/failing-reporter.cjs')}`,
    fixtures.path('bench-runner/a.cjs'),
  ]);
  assert.strictEqual(result.status, 1);
  assert.match(result.stderr, /benchmark reporter failed/);
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
