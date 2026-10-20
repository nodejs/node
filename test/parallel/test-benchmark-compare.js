'use strict';

require('../common');

const assert = require('node:assert');
const { spawnSyncAndExitWithoutError } = require('../common/child_process');
const { readFileSync } = require('node:fs');
const path = require('node:path');
const tmpdir = require('../common/tmpdir');

const compare = path.resolve(__dirname, '../../benchmark/compare.js');

tmpdir.refresh();

const csv = tmpdir.resolve('compare.csv');
spawnSyncAndExitWithoutError(process.execPath, [
  compare,
  '--old', process.execPath,
  '--new', process.execPath,
  '--runs', '1',
  '--filter', 'buffer-compare-offset.js',
  '--set', 'method=offset',
  '--set', 'size=16',
  '--set', 'n=1',
  '--no-progress',
  '--analyze',
  '--csv', csv,
  'buffers',
], {
  encoding: 'utf8',
  timeout: 30_000,
}, {
  stderr: '',
  stdout(stdout) {
    assert.match(stdout, /confidence\s+improvement\s+accuracy/);
    assert.doesNotMatch(stdout, /"binary","filename"/);
  },
});

const lines = readFileSync(csv, 'utf8').trim().split('\n');
const filename = path.join('buffers', 'buffer-compare-offset.js');
assert.strictEqual(lines[0],
                   '"binary","filename","configuration","rate","time"');
assert.strictEqual(lines.length, 3);
assert(lines[1].startsWith(`"old","${filename}",`));
assert(lines[2].startsWith(`"new","${filename}",`));
