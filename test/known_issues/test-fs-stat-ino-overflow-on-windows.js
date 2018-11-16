'use strict';

const common = require('../common');
const assert = require('assert');

if (!common.isWindows)
  assert.fail('Code should fail only on Windows.');

const fs = require('fs');
const promiseFs = require('fs').promises;
const path = require('path');
const tmpdir = require('../common/tmpdir');
const { isDate } = require('util').types;

tmpdir.refresh();

let testIndex = 0;

function getFilename() {
  const filename = path.join(tmpdir.path, `test-file-${++testIndex}`);
  fs.writeFileSync(filename, 'test');
  return filename;
}

function verifyStats(bigintStats, numStats) {
  assert.ok(Number.isSafeInteger(numStats.ino));
  assert.strictEqual(bigintStats.ino, BigInt(numStats.ino));
}

{
  const filename = getFilename();
  const bigintStats = fs.statSync(filename, { bigint: true });
  const numStats = fs.statSync(filename);
  verifyStats(bigintStats, numStats);
}

{
  const filename = __filename;
  const bigintStats = fs.statSync(filename, { bigint: true });
  const numStats = fs.statSync(filename);
  verifyStats(bigintStats, numStats);
}

{
  const filename = __dirname;
  const bigintStats = fs.statSync(filename, { bigint: true });
  const numStats = fs.statSync(filename);
  verifyStats(bigintStats, numStats);
}

{
  const filename = getFilename();
  const fd = fs.openSync(filename, 'r');
  const bigintStats = fs.fstatSync(fd, { bigint: true });
  const numStats = fs.fstatSync(fd);
  verifyStats(bigintStats, numStats);
  fs.closeSync(fd);
}

{
  const filename = getFilename();
  fs.stat(filename, { bigint: true }, (err, bigintStats) => {
    fs.stat(filename, (err, numStats) => {
      verifyStats(bigintStats, numStats);
    });
  });
}

{
  const filename = getFilename();
  const fd = fs.openSync(filename, 'r');
  fs.fstat(fd, { bigint: true }, (err, bigintStats) => {
    fs.fstat(fd, (err, numStats) => {
      verifyStats(bigintStats, numStats);
      fs.closeSync(fd);
    });
  });
}

(async function() {
  const filename = getFilename();
  const bigintStats = await promiseFs.stat(filename, { bigint: true });
  const numStats = await promiseFs.stat(filename);
  verifyStats(bigintStats, numStats);
})();

(async function() {
  const filename = getFilename();
  const handle = await promiseFs.open(filename, 'r');
  const bigintStats = await handle.stat({ bigint: true });
  const numStats = await handle.stat();
  verifyStats(bigintStats, numStats);
  await handle.close();
})();
