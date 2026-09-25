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
const CHUNK_SIZE = 16 * 1024 * 1024;
const STREAMED_MEMBER_SIZE = 4 * GiB + CHUNK_SIZE;
const TAIL_MEMBER_SIZE = 64 * 1024;
// The leading member needs Zip64 sizes; the small member after it needs a
// Zip64 offset. Only one member has to be large to exercise both paths.
const REQUIRED_FREE_BYTES = 8 * GiB;
const CENTRAL_FILE_HEADER_SIGNATURE = Buffer.from([0x50, 0x4b, 0x01, 0x02]);

async function* repeatingChunks(totalSize, seed) {
  const chunk = Buffer.alloc(Math.min(CHUNK_SIZE, totalSize), seed);
  let remaining = totalSize;
  while (remaining > 0) {
    const size = Math.min(chunk.length, remaining);
    remaining -= size;
    yield size === chunk.length ? chunk : chunk.subarray(0, size);
  }
}

function assertZip64Extra(buffer, offset, values) {
  assert.strictEqual(buffer.readUInt16LE(offset), 0x0001);
  assert.strictEqual(buffer.readUInt16LE(offset + 2), values.length * 8);
  for (let i = 0; i < values.length; i++) {
    assert.strictEqual(buffer.readBigUInt64LE(offset + 4 + i * 8), BigInt(values[i]));
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
    const entries = [
      zlib.ZipEntry.createStream('streamed.bin', repeatingChunks(STREAMED_MEMBER_SIZE, 0xaa), {
        method: 'store',
      }),
      zlib.ZipEntry.createStream('tail.bin', repeatingChunks(TAIL_MEMBER_SIZE, 2), {
        method: 'store',
      }),
    ];

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

    const reader = await fs.open(archivePath, 'r');
    try {
      // Read only the leading local header and the archive tail. The two
      // central headers and the trailer fit comfortably in these 1024 bytes.
      const local = Buffer.alloc(30);
      const tail = Buffer.alloc(1024);
      await reader.read(local, 0, local.length, 0);
      await reader.read(tail, 0, tail.length, stat.size - tail.length);
      assert.strictEqual(local.readUInt32LE(0), 0x04034b50);
      const tailOffset = local.length + local.readUInt16LE(26) +
        local.readUInt16LE(28) + STREAMED_MEMBER_SIZE + 24; // Zip64 data descriptor.
      assert.ok(tailOffset > 0xffffffff);

      const bigCentral = tail.indexOf(CENTRAL_FILE_HEADER_SIGNATURE);
      assert.notStrictEqual(bigCentral, -1);
      assert.strictEqual(tail.readUInt32LE(bigCentral + 20), 0xffffffff);
      assert.strictEqual(tail.readUInt32LE(bigCentral + 24), 0xffffffff);
      assertZip64Extra(tail, bigCentral + 46 + tail.readUInt16LE(bigCentral + 28),
                       [STREAMED_MEMBER_SIZE, STREAMED_MEMBER_SIZE]);

      const tailCentral = tail.indexOf(CENTRAL_FILE_HEADER_SIGNATURE, bigCentral + 4);
      assert.notStrictEqual(tailCentral, -1);
      assert.strictEqual(tail.readUInt32LE(tailCentral + 42), 0xffffffff);
      assertZip64Extra(tail, tailCentral + 46 + tail.readUInt16LE(tailCentral + 28),
                       [tailOffset]);
    } finally {
      await reader.close();
    }

    const zip = await zlib.ZipFile.open(archivePath);
    try {
      assert.strictEqual(zip.size, 2);

      let seen = 0;
      for await (const chunk of await zip.stream('streamed.bin')) {
        seen += chunk.length;
        assert.strictEqual(chunk[0], 0xaa);
      }
      assert.strictEqual(seen, STREAMED_MEMBER_SIZE);

      let tailSeen = 0;
      for await (const chunk of await zip.stream('tail.bin')) {
        tailSeen += chunk.length;
        assert.strictEqual(chunk[0], 2);
      }
      assert.strictEqual(tailSeen, TAIL_MEMBER_SIZE);

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
        if (reserialized === 0) {
          assert.strictEqual(chunk.readUInt16LE(6) & 0x08, 0); // No data descriptor.
          assert.strictEqual(chunk.readUInt32LE(18), 0xffffffff);
          assert.strictEqual(chunk.readUInt32LE(22), 0xffffffff);
          assertZip64Extra(chunk, 30 + chunk.readUInt16LE(26),
                           [STREAMED_MEMBER_SIZE, STREAMED_MEMBER_SIZE]);
        }
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
