'use strict';

// Lifecycle-hardening regression tests for node:zlib ZipFile (the on-disk,
// fd-backed reader/writer). Each asserts the secure behavior, so it fails on
// the pre-fix code and passes once its fix lands.
//
//  1. A read in flight when close() is called must complete on a live
//     descriptor - close() must not pull the fd out from under it (which would
//     surface as EBADF, or worse read another file once the fd number is
//     reused).
//  2. If the central-directory rewrite fails after an add() has written the
//     member bytes, the in-memory state and the on-disk archive must be rolled
//     back to exactly what they were before the call, not left half-updated.

require('../common');
const assert = require('assert');
const { test } = require('node:test');
const zlib = require('zlib');
const fs = require('fs');
const tmpdir = require('../common/tmpdir');

const SIG_CENTRAL = 0x02014b50;
const SIG_EOCD = 0x06054b50;

function writeArchive(file, entries) {
  const chunks = [];
  for (const chunk of zlib.createZipArchiveSync(entries)) chunks.push(chunk);
  fs.writeFileSync(file, Buffer.concat(chunks));
}

// 1. A read that is in flight when the ZipFile is closed still completes.
test('close() waits for an in-flight read instead of closing under it', async () => {
  tmpdir.refresh();
  const file = tmpdir.resolve('inflight.zip');
  const payload = Buffer.alloc(8 * 1024 * 1024, 0x5a);
  writeArchive(file, [zlib.ZipEntry.createSync('big', payload, { method: 'store' })]);

  const zf = zlib.ZipFile.openSync(file);
  const entry = zf.getSync('big');
  const reading = entry.content(); // In flight; do not await yet
  await zf.close(); // Must wait for the read, not close the fd under it
  const data = await reading; // Must resolve with correct bytes, not reject EBADF
  assert.strictEqual(data.length, payload.length);
  assert.ok(data.equals(payload));
});

// 2. A failed central-directory rewrite during add() is rolled back.
test('a failed directory rewrite during addEntrySync is rolled back', () => {
  tmpdir.refresh();
  const file = tmpdir.resolve('addfail.zip');
  writeArchive(file, [zlib.ZipEntry.createSync('first.txt', Buffer.from('original'),
                                               { method: 'store' })]);

  const zf = zlib.ZipFile.openSync(file, { writable: true });
  const toAdd = zlib.ZipEntry.createSync('second.txt', Buffer.from('added'), { method: 'store' });

  // Fail only the first central-directory write (its buffer starts with the
  // central-header or EOCD signature); the member bytes start with the local
  // header signature and pass through, and the rollback rewrite that follows
  // succeeds so the on-disk archive is restored.
  const realWriteSync = fs.writeSync;
  let failNextDirectoryWrite = true;
  fs.writeSync = function(fd, buffer, offset, length, position) {
    if (failNextDirectoryWrite && Buffer.isBuffer(buffer) && buffer.length - offset >= 4) {
      const sig = buffer.readUInt32LE(offset);
      if (sig === SIG_CENTRAL || sig === SIG_EOCD) {
        failNextDirectoryWrite = false;
        const err = new Error('ENOSPC: simulated no space left on device');
        err.code = 'ENOSPC';
        throw err;
      }
    }
    return realWriteSync.call(fs, fd, buffer, offset, length, position);
  };
  try {
    assert.throws(() => zf.addEntrySync(toAdd), { code: 'ENOSPC' });
  } finally {
    fs.writeSync = realWriteSync;
  }

  // In-memory: the half-added entry is gone and the original is still readable.
  assert.ok(!zf.has('second.txt'));
  assert.strictEqual(zf.getSync('first.txt').contentSync().toString(), 'original');
  zf.closeSync();

  // On disk: reopening shows the original, uncorrupted archive.
  const reopened = zlib.ZipFile.openSync(file);
  assert.ok(reopened.has('first.txt'));
  assert.ok(!reopened.has('second.txt'));
  assert.strictEqual(reopened.getSync('first.txt').contentSync().toString(), 'original');
  reopened.closeSync();
});
