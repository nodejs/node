'use strict';

const common = require('../common');
const assert = require('assert');
const fs = require('fs');
const promiseFs = require('fs').promises;
const path = require('path');
const tmpdir = require('../common/tmpdir');
const { isDate } = require('util').types;
const { inspect } = require('util');

tmpdir.refresh();

let testIndex = 0;

function getFilename() {
  const filename = path.join(tmpdir.path, `test-file-${++testIndex}`);
  fs.writeFileSync(filename, 'test');
  return filename;
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
      assert.strictEqual(
        bigintStats[key], BigInt(val),
        `${inspect(bigintStats[key])} !== ${inspect(BigInt(val))}\n` +
        `key=${key}, val=${val}`
      );
    } else {
      assert(
        Math.abs(Number(bigintStats[key]) - val) < 1,
        `${key} is not a safe integer, difference should < 1.\n` +
        `Number version ${val}, BigInt version ${bigintStats[key]}n`);
    }
  }
}

{
  const filename = getFilename();
  const bigintStats = fs.statSync(filename, { bigint: true });
  const numStats = fs.statSync(filename);
  verifyStats(bigintStats, numStats);
}

if (!common.isWindows) {
  const filename = getFilename();
  const link = `${filename}-link`;
  fs.symlinkSync(filename, link);
  const bigintStats = fs.lstatSync(link, { bigint: true });
  const numStats = fs.lstatSync(link);
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

if (!common.isWindows) {
  const filename = getFilename();
  const link = `${filename}-link`;
  fs.symlinkSync(filename, link);
  fs.lstat(link, { bigint: true }, (err, bigintStats) => {
    fs.lstat(link, (err, numStats) => {
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

if (!common.isWindows) {
  (async function() {
    const filename = getFilename();
    const link = `${filename}-link`;
    fs.symlinkSync(filename, link);
    const bigintStats = await promiseFs.lstat(link, { bigint: true });
    const numStats = await promiseFs.lstat(link);
    verifyStats(bigintStats, numStats);
  })();
}

(async function() {
  const filename = getFilename();
  const handle = await promiseFs.open(filename, 'r');
  const bigintStats = await handle.stat({ bigint: true });
  const numStats = await handle.stat();
  verifyStats(bigintStats, numStats);
  await handle.close();
})();
