'use strict';

const common = require('../common');
const assert = require('assert');
const { spawnSync } = require('child_process');
const fixtures = require('../common/fixtures');

const result = spawnSync(process.execPath, [
  '--no-warnings', '--experimental-bench', '--bench',
  '--require', fixtures.path('bench-runner/delayed-ack.cjs'),
  '--bench-reporter=json',
  fixtures.path('bench-runner/acknowledged-records.mjs'),
], { encoding: 'utf8', timeout: common.platformTimeout(30_000) });

assert.ifError(result.error);
assert.strictEqual(result.signal, null);
assert.strictEqual(result.status, 0, result.stdout + result.stderr);
assert.strictEqual(result.stderr, '');
const records = result.stdout.trim().split('\n').map((line) => JSON.parse(line));
const diagnostics = records.filter(({ type }) => type === 'bench:diagnostic');
assert.strictEqual(diagnostics.length, 32);
assert(diagnostics.every(({ data }) => /^acknowledged 10\d{3}$/.test(data.message)));
assert.strictEqual(records.at(-1).data.success, true);
