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
    '--bench-reporter=json',
    fixtures.path('bench-runner/acknowledged-records.mjs'),
  ]);
  assert.strictEqual(result.status, 0);
  assert.strictEqual(result.stderr, '');
  const records = parseRecords(result);
  const diagnostics = records.filter(
    ({ type }) => type === 'bench:diagnostic');
  assert.strictEqual(diagnostics.length, 32);
  assert(diagnostics.every(
    ({ data }) => /^acknowledged 10\d{3}$/.test(data.message)));
  assert.strictEqual(records.at(-1).data.success, true);
}
