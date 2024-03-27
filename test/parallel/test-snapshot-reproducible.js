'use strict';

require('../common');
const { spawnSyncAndExitWithoutError } = require('../common/child_process');
const tmpdir = require('../common/tmpdir');
const fs = require('fs');
const assert = require('assert');
const fixtures = require('../common/fixtures');

function generateSnapshot() {
  tmpdir.refresh();

  spawnSyncAndExitWithoutError(
    process.execPath,
    [
      '--random_seed=42',
      '--predictable',
      '--build-snapshot',
      'node:generate_default_snapshot',
    ],
    {
      cwd: tmpdir.path
    }
  );
  const blobPath = tmpdir.resolve('snapshot.blob');
  return fs.readFileSync(blobPath);
}

const buf1 = generateSnapshot();
const buf2 = generateSnapshot();
const diff = [];
let offset = 0;
const step = 16;
do {
  const length = Math.min(buf1.length - offset, step);
  const slice1 = buf1.slice(offset, offset + length).toString('hex');
  const slice2 = buf2.slice(offset, offset + length).toString('hex');
  if (slice1 != slice2) {
    diff.push({offset, slice1, slice2});
  }
  offset += length;
} while (offset < buf1.length);

assert.strictEqual(offset, buf1.length);
if (offset < buf2.length) {
  const length = Math.min(buf2.length - offset, step);
  const slice2 = buf2.slice(offset, offset + length).toString('hex');
  diff.push({offset, slice1: '', slice2});
  offset += length;
} while (offset < buf2.length);

assert.deepStrictEqual(diff, [], 'Built-in snapshot should not change in different builds.');
assert.strictEqual(buf1.length, buf2.length);
