import * as common from '../common/index.mjs';

// Test timestamps returned by fsPromises.stat and fs.statSync

import fs from 'node:fs';
import fsPromises from 'node:fs/promises';
import assert from 'node:assert';
import tmpdir from '../common/tmpdir.js';

// On some platforms (for example, ppc64) boundaries are tighter
// than usual. If we catch these errors, skip corresponding test.
const ignoredErrors = new Set(['EINVAL', 'EOVERFLOW']);

tmpdir.refresh();
const filepath = tmpdir.resolve('timestamp');

await (await fsPromises.open(filepath, 'w')).close();

// Perform a trivial check to determine if filesystem supports setting
// and retrieving atime and mtime. If it doesn't, skip the test.
await fsPromises.utimes(filepath, 2, 2);
const { atimeMs, mtimeMs } = await fsPromises.stat(filepath);
if (atimeMs !== 2000 || mtimeMs !== 2000) {
  common.skip(`Unsupported filesystem (atime=${atimeMs}, mtime=${mtimeMs})`);
}

// Date might round down timestamp
function closeEnough(actual, expected, margin) {
  // On ppc64, value is rounded to seconds
  if (process.arch === 'ppc64') {
    margin += 1000;
  }

  // Filesystems without support for timestamps before 1970-01-01, such as NFSv3,
  // should return 0 for negative numbers. Do not treat it as error.
  if (actual === 0 && expected < 0) {
    console.log(`ignored 0 while expecting ${expected}`);
    return;
  }

  assert.ok(Math.abs(Number(actual - expected)) < margin,
            `expected ${expected} ± ${margin}, got ${actual}`);
}

async function runTest(atime, mtime, margin = 0) {
  margin += Number.EPSILON;
  try {
    await fsPromises.utimes(filepath, new Date(atime), new Date(mtime));
  } catch (e) {
    if (ignoredErrors.has(e.code)) return;
    throw e;
  }

  const stats = await fsPromises.stat(filepath);
  closeEnough(stats.atimeMs, atime, margin);
  closeEnough(stats.mtimeMs, mtime, margin);
  closeEnough(stats.atime.getTime(), new Date(atime).getTime(), margin);
  closeEnough(stats.mtime.getTime(), new Date(mtime).getTime(), margin);

  const statsBigint = await fsPromises.stat(filepath, { bigint: true });
  closeEnough(statsBigint.atimeMs, BigInt(atime), margin);
  closeEnough(statsBigint.mtimeMs, BigInt(mtime), margin);
  closeEnough(statsBigint.atime.getTime(), new Date(atime).getTime(), margin);
  closeEnough(statsBigint.mtime.getTime(), new Date(mtime).getTime(), margin);

  const statsSync = fs.statSync(filepath);
  closeEnough(statsSync.atimeMs, atime, margin);
  closeEnough(statsSync.mtimeMs, mtime, margin);
  closeEnough(statsSync.atime.getTime(), new Date(atime).getTime(), margin);
  closeEnough(statsSync.mtime.getTime(), new Date(mtime).getTime(), margin);

  const statsSyncBigint = fs.statSync(filepath, { bigint: true });
  closeEnough(statsSyncBigint.atimeMs, BigInt(atime), margin);
  closeEnough(statsSyncBigint.mtimeMs, BigInt(mtime), margin);
  closeEnough(statsSyncBigint.atime.getTime(), new Date(atime).getTime(), margin);
  closeEnough(statsSyncBigint.mtime.getTime(), new Date(mtime).getTime(), margin);
}

// Too high/low numbers produce too different results on different platforms
{
  // TODO(LiviaMedeiros): investigate outdated stat time on FreeBSD.
  // On Windows, filetime is stored and handled differently. Supporting dates
  // after Y2038 is preferred over supporting dates before 1970-01-01.
  if (!common.isFreeBSD && !common.isWindows) {
    await runTest(-40691, -355, 1); // Potential precision loss on 32bit
    await runTest(-355, -40691, 1);  // Potential precision loss on 32bit
    await runTest(-1, -1);
  }
  await runTest(0, 0);
  await runTest(1, 1);
  await runTest(355, 40691, 1); // Precision loss on 32bit
  await runTest(40691, 355, 1); // Precision loss on 32bit
  await runTest(1713037251360, 1713037251360, 1); // Precision loss
}
