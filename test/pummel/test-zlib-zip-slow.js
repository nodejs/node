'use strict';

// This test writes and reads back a ZIP archive whose total size exceeds
// 4 GiB, to exercise the Zip64 promotion that is triggered by an offset or
// size overflowing the classic 32-bit fields (as opposed to test-zlib-zip-
// zip64.js in test/parallel, which triggers Zip64 through the 16-bit entry
// count instead). It needs several GiB of free disk space and is too slow
// for the default test run, hence living in test/pummel rather than
// test/parallel.

const common = require('../common');

const assert = require('node:assert');
const zlib = require('node:zlib');
const fs = require('node:fs/promises');
const path = require('node:path');
const tmpdir = require('../common/tmpdir');
const { test } = require('node:test');

tmpdir.refresh();

const GiB = 1024 * 1024 * 1024;
const MEMBER_SIZE = 500 * 1024 * 1024; // Four ~500 MiB stored members...
const STORED_MEMBER_COUNT = 4;
const STREAMED_MEMBER_SIZE = 4.5 * GiB; // ...plus one >4 GiB streamed member:
// the total archive size (~6.5 GiB) pushes offsets over the 4 GiB Zip64
// threshold, and the streamed member's own sizes exceed 32 bits too, so the
// per-entry Zip64 size fields (central header and data descriptor) are
// exercised as well as the offset promotion. Required free space includes
// generous slack over that total.
const REQUIRED_FREE_BYTES = 12 * GiB;
const CHUNK_SIZE = 16 * 1024 * 1024;

function fillChunk(seed) {
  const chunk = Buffer.allocUnsafe(CHUNK_SIZE);
  chunk.fill(seed & 0xff);
  return chunk;
}

async function* repeatingChunks(totalSize, seed) {
  let remaining = totalSize;
  while (remaining > 0) {
    const size = Math.min(CHUNK_SIZE, remaining);
    const chunk = fillChunk(seed);
    remaining -= size;
    yield size === chunk.length ? chunk : chunk.subarray(0, size);
  }
}

test('an archive larger than 4 GiB round-trips and triggers Zip64 via offset', async () => {
  let free;
  try {
    const stats = await fs.statfs(tmpdir.path);
    free = stats.bavail * stats.bsize;
  } catch {
    free = undefined;
  }
  if (free !== undefined && free < REQUIRED_FREE_BYTES) {
    common.skip(`insufficient disk space in ${tmpdir.path} for a >4 GiB archive test`);
    return;
  }

  const dir = await fs.mkdtemp(path.join(tmpdir.path, 'zlib-zip-slow-'));
  const archivePath = path.join(dir, 'large.zip');
  try {
    const entries = [];
    for (let i = 0; i < STORED_MEMBER_COUNT; i++) {
      entries.push(zlib.ZipEntry.createStream(`stored-${i}.bin`, repeatingChunks(MEMBER_SIZE, i), {
        method: 'store',
      }));
    }
    entries.push(zlib.ZipEntry.createStream('streamed.bin', repeatingChunks(STREAMED_MEMBER_SIZE, 0xaa), {
      method: 'store',
    }));

    const handle = await fs.open(archivePath, 'w');
    try {
      for await (const chunk of zlib.createZipArchive(entries)) {
        await handle.write(chunk);
      }
    } finally {
      await handle.close();
    }

    const stat = await fs.stat(archivePath);
    assert.ok(stat.size > 4 * GiB, `archive is only ${stat.size} bytes`);

    const zip = await zlib.ZipFile.open(archivePath);
    try {
      assert.strictEqual(zip.size, STORED_MEMBER_COUNT + 1);

      let seen = 0;
      for await (const chunk of await zip.stream('streamed.bin')) {
        seen += chunk.length;
        assert.strictEqual(chunk[0], 0xaa);
      }
      assert.strictEqual(seen, STREAMED_MEMBER_SIZE);

      let storedSeen = 0;
      for await (const chunk of await zip.stream('stored-2.bin')) {
        storedSeen += chunk.length;
        assert.strictEqual(chunk[0], 2);
      }
      assert.strictEqual(storedSeen, MEMBER_SIZE);

      // The streamed member's sizes genuinely exceed 32 bits (stored, so
      // compressed === uncompressed), which the reader must have resolved
      // through the central header's Zip64 extra field.
      const big = await zip.get('streamed.bin');
      assert.strictEqual(big.size, STREAMED_MEMBER_SIZE);
      assert.strictEqual(big.compressedSize, STREAMED_MEMBER_SIZE);

      // Re-serialize the >4 GiB file-backed member through the archive
      // writer, discarding the output: this exercises the non-streaming
      // Zip64 local-header path (real 64-bit sizes up front, no data
      // descriptor) without needing a second copy on disk.
      let reserialized = 0;
      for await (const chunk of zlib.createZipArchive([big])) {
        reserialized += chunk.length;
      }
      assert.ok(reserialized > STREAMED_MEMBER_SIZE,
                `re-serialized only ${reserialized} bytes`);
    } finally {
      await zip.close();
    }
  } finally {
    await fs.rm(dir, { recursive: true, force: true });
  }
}, { timeout: 30 * 60 * 1000 });
