'use strict';

const common = require('../common');
const assert = require('assert');

if (!(common.isWindows || common.isSunOS || common.isAIX))
  assert.fail('Code should fail only on Windows.');

const fs = require('fs');
const promiseFs = require('fs').promises;
const path = require('path');
const tmpdir = require('../common/tmpdir');

tmpdir.refresh();

let testIndex = 0;

function getFilename() {
  const filename = path.join(tmpdir.path, `test-file-${++testIndex}`);
  fs.writeFileSync(filename, 'test');
  return filename;
}

function verifyStats(bigintStats, numStats) {
  const keys = [
    'mode', 'nlink', 'uid', 'gid', 'size',
    // `rdev` can overflow on AIX and smartOS
    'rdev',
    // `dev` can overflow on AIX
    'dev',
    // `ino` can overflow on Windows
    'ino',
  ];
  const nStats = keys.reduce(
    (s, k) => Object.assign(s, { [k]: String(numStats[k]) }),
    {}
  );
  const bStats = keys.reduce(
    (s, k) => Object.assign(s, { [k]: String(bigintStats[k]) }),
    {}
  );
  assert.deepStrictEqual(nStats, bStats);
  for (const key of keys) {
    assert.ok(
      Number.isSafeInteger(numStats[key]),
      `numStats.${key}: ${numStats[key]} is not a safe integer`
    );
  }
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
