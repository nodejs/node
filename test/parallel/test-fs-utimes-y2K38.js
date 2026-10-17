'use strict';
const common = require('../common');

const tmpdir = require('../common/tmpdir');
tmpdir.refresh();

const assert = require('assert');
const fs = require('fs');

// Ref: https://github.com/nodejs/node/issues/13255
const path = `${tmpdir.path}/test-utimes-precision`;
fs.writeFileSync(path, '');

if (common.isWindows) {
  // This value would get converted to (double)1713037251359.9998
  const truncate_mtime = 1713037251360;
  fs.utimesSync(path, truncate_mtime / 1000, truncate_mtime / 1000);
  const truncate_stats = fs.statSync(path);
  assert.strictEqual(truncate_stats.mtime.getTime(), truncate_mtime);

  // Libuv stores Windows timestamps in a 32-bit `long tv_sec`, so Y2K38
  // times can not be represented. Negative values are pre-epoch times.
  common.skip('Y2K38 is not supported on Windows');
}

// Check for Y2K38 support.
{
  const testFilePath = `${tmpdir.path}/y2k38-test`;
  const testFileDate = '204001020304';
  const { spawnSync } = require('child_process');
  const touchResult = spawnSync('touch',
                                ['-t', testFileDate, testFilePath],
                                { encoding: 'utf8' });
  if (touchResult.status !== 0) {
    common.skip('File system appears to lack Y2K38 support (touch failed)');
  }

  // On some file systems that lack Y2K38 support, `touch` will succeed but
  // the time will be incorrect.
  const dateResult = spawnSync('date',
                               ['-r', testFilePath, '+%Y%m%d%H%M'],
                               { encoding: 'utf8' });
  if (dateResult.status === 0) {
    if (dateResult.stdout.trim() !== testFileDate) {
      common.skip('File system appears to lack Y2k38 support (date failed)');
    }
  } else {
    // On some platforms `date` may not support the `-r` option. Usually
    // this will result in a non-zero status and usage information printed.
    // In this case optimistically proceed -- the earlier `touch` succeeded
    // but validation that the file has the correct time is not easily possible.
    assert.match(dateResult.stderr, /[Uu]sage:/);
  }
}

const Y2K38_mtime = 2 ** 31;
fs.utimesSync(path, Y2K38_mtime, Y2K38_mtime);
const Y2K38_stats = fs.statSync(path);
assert.strictEqual(Y2K38_stats.mtime.getTime() / 1000, Y2K38_mtime);
