import * as common from '../common/index.mjs';

// Test `Temporal.Instant`s returned by fsPromises.stat and fs.statSync

import fs from 'node:fs';
import fsPromises from 'node:fs/promises';
import assert from 'node:assert';
import tmpdir from '../common/tmpdir.js';

if (!common.hasTemporal) {
  common.skip('Temporal support unavailable');
}

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
  common.skip(`Unsupported filesystem (atimeMs=${atimeMs}, mtimeMs=${mtimeMs})`);
}

// Allow delta due to precision loss or platform-specific nuances
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

// Ensure that accessed atime and mtime are enumerable
function validateEnumerability(stats) {
  const keys = Object.keys(stats);
  assert.ok(keys.includes('atimeInstant'));
  assert.ok(keys.includes('mtimeInstant'));
}

async function runTest(atimeMs, mtimeMs, margin = 0) {
  margin += Number.EPSILON;

  // TODO(LiviaMedeiros): use bigint nanoseconds once `utimes()` supports Temporal
  const atime = atimeMs / 1000;
  const mtime = mtimeMs / 1000;
  try {
    await fsPromises.utimes(filepath, atime, mtime);
  } catch (e) {
    if (ignoredErrors.has(e.code)) return;
    throw e;
  }

  const stats = await fsPromises.stat(filepath);
  closeEnough(stats.atimeMs, atimeMs, margin);
  closeEnough(stats.mtimeMs, mtimeMs, margin);
  closeEnough(stats.atimeInstant.epochMilliseconds, atimeMs, margin);
  closeEnough(stats.mtimeInstant.epochMilliseconds, mtimeMs, margin);
  validateEnumerability(stats);

  const statsBigint = await fsPromises.stat(filepath, { bigint: true });
  closeEnough(statsBigint.atimeMs, BigInt(atimeMs), margin);
  closeEnough(statsBigint.mtimeMs, BigInt(mtimeMs), margin);
  closeEnough(statsBigint.atimeInstant.epochMilliseconds, atimeMs, margin);
  closeEnough(statsBigint.mtimeInstant.epochMilliseconds, mtimeMs, margin);
  validateEnumerability(statsBigint);

  const statsSync = fs.statSync(filepath);
  closeEnough(statsSync.atimeMs, atimeMs, margin);
  closeEnough(statsSync.mtimeMs, mtimeMs, margin);
  closeEnough(statsSync.atimeInstant.epochMilliseconds, atimeMs, margin);
  closeEnough(statsSync.mtimeInstant.epochMilliseconds, mtimeMs, margin);
  validateEnumerability(statsSync);

  const statsSyncBigint = fs.statSync(filepath, { bigint: true });
  closeEnough(statsSyncBigint.atimeMs, BigInt(atimeMs), margin);
  closeEnough(statsSyncBigint.mtimeMs, BigInt(mtimeMs), margin);
  closeEnough(statsSyncBigint.atimeInstant.epochMilliseconds, atimeMs, margin);
  closeEnough(statsSyncBigint.mtimeInstant.epochMilliseconds, mtimeMs, margin);
  validateEnumerability(statsSyncBigint);
}

// Too high/low numbers produce too different results on different platforms
{
  await runTest(0, 0);
  await runTest(1, 1);
  await runTest(355, 40691, 1); // Precision loss on 32bit
  await runTest(40691, 355, 1); // Precision loss on 32bit
  await runTest(1713037251360, 1713037251360, 1); // Precision loss
}
