'use strict';

const common = require('../common');
const assert = require('assert');
const fs = require('fs');
const promiseFs = require('fs').promises;
const path = require('path');
const tmpdir = require('../common/tmpdir');
const { isDate } = require('util').types;

common.crashOnUnhandledRejection();
tmpdir.refresh();

const fn = path.join(tmpdir.path, 'test-file');
fs.writeFileSync(fn, 'test');

let link;
if (!common.isWindows) {
  link = path.join(tmpdir.path, 'symbolic-link');
  fs.symlinkSync(fn, link);
}

function verifyStats(bigintStats, numStats) {
  for (const key of Object.keys(numStats)) {
    const val = numStats[key];
    if (isDate(val)) {
      const time = val.getTime();
      const time2 = bigintStats[key].getTime();
      assert(
        Math.abs(time - time2) < 2,
        `difference of ${key}.getTime() should < 2.\n` +
        `Number version ${time}, BigInt version ${time2}n`);
    } else if (key === 'mode') {
      assert.strictEqual(bigintStats[key], BigInt(val));
      assert.strictEqual(
        bigintStats.isBlockDevice(),
        numStats.isBlockDevice()
      );
      assert.strictEqual(
        bigintStats.isCharacterDevice(),
        numStats.isCharacterDevice()
      );
      assert.strictEqual(
        bigintStats.isDirectory(),
        numStats.isDirectory()
      );
      assert.strictEqual(
        bigintStats.isFIFO(),
        numStats.isFIFO()
      );
      assert.strictEqual(
        bigintStats.isFile(),
        numStats.isFile()
      );
      assert.strictEqual(
        bigintStats.isSocket(),
        numStats.isSocket()
      );
      assert.strictEqual(
        bigintStats.isSymbolicLink(),
        numStats.isSymbolicLink()
      );
    } else if (common.isWindows && (key === 'blksize' || key === 'blocks')) {
      assert.strictEqual(bigintStats[key], undefined);
      assert.strictEqual(numStats[key], undefined);
    } else if (Number.isSafeInteger(val)) {
      assert.strictEqual(bigintStats[key], BigInt(val));
    } else {
      assert(
        Math.abs(Number(bigintStats[key]) - val) < 1,
        `${key} is not a safe integer, difference should < 1.\n` +
        `Number version ${val}, BigInt version ${bigintStats[key]}n`);
    }
  }
}

{
  const bigintStats = fs.statSync(fn, { bigint: true });
  const numStats = fs.statSync(fn);
  verifyStats(bigintStats, numStats);
}

if (!common.isWindows) {
  const bigintStats = fs.lstatSync(link, { bigint: true });
  const numStats = fs.lstatSync(link);
  verifyStats(bigintStats, numStats);
}

{
  const fd = fs.openSync(fn, 'r');
  const bigintStats = fs.fstatSync(fd, { bigint: true });
  const numStats = fs.fstatSync(fd);
  verifyStats(bigintStats, numStats);
  fs.closeSync(fd);
}

{
  fs.stat(fn, { bigint: true }, (err, bigintStats) => {
    fs.stat(fn, (err, numStats) => {
      verifyStats(bigintStats, numStats);
    });
  });
}

if (!common.isWindows) {
  fs.lstat(link, { bigint: true }, (err, bigintStats) => {
    fs.lstat(link, (err, numStats) => {
      verifyStats(bigintStats, numStats);
    });
  });
}

{
  const fd = fs.openSync(fn, 'r');
  fs.fstat(fd, { bigint: true }, (err, bigintStats) => {
    fs.fstat(fd, (err, numStats) => {
      verifyStats(bigintStats, numStats);
      fs.closeSync(fd);
    });
  });
}

(async function() {
  const bigintStats = await promiseFs.stat(fn, { bigint: true });
  const numStats = await promiseFs.stat(fn);
  verifyStats(bigintStats, numStats);
})();

if (!common.isWindows) {
  (async function() {
    const bigintStats = await promiseFs.lstat(link, { bigint: true });
    const numStats = await promiseFs.lstat(link);
    verifyStats(bigintStats, numStats);
  })();
}

(async function() {
  const handle = await promiseFs.open(fn, 'r');
  const bigintStats = await handle.stat({ bigint: true });
  const numStats = await handle.stat();
  verifyStats(bigintStats, numStats);
  await handle.close();
})();
