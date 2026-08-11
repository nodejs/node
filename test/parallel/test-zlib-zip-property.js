'use strict';

require('../common');

const assert = require('node:assert');
const zlib = require('node:zlib');
const fs = require('node:fs/promises');
const path = require('node:path');
const tmpdir = require('../common/tmpdir');
const { test } = require('node:test');

tmpdir.refresh();

// A small, seeded PRNG so failures are reproducible without pulling in a
// dependency.
function mulberry32(seed) {
  let state = seed >>> 0;
  return function() {
    state = (state + 0x6d2b79f5) >>> 0;
    let t = state;
    t = Math.imul(t ^ (t >>> 15), t | 1);
    t ^= t + Math.imul(t ^ (t >>> 7), t | 61);
    return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
  };
}

function randomBuffer(random, length) {
  const buf = Buffer.allocUnsafe(length);
  for (let i = 0; i < length; i++) buf[i] = Math.floor(random() * 256);
  return buf;
}

function randomName(random, index) {
  const unicodeBits = ['a', 'é', '日', '🙂', 'z'];
  const segment = unicodeBits[Math.floor(random() * unicodeBits.length)];
  return `dir-${index}/${segment}-${index}.bin`;
}

async function buildTree(random, count) {
  const specs = [];
  for (let i = 0; i < count; i++) {
    const size = Math.floor(random() * 4096);
    specs.push({
      name: randomName(random, i),
      data: randomBuffer(random, size),
      method: random() < 0.5 ? 'deflate' : 'store',
      mode: random() < 0.5 ? 0o644 : 0o600,
      modified: new Date(2000 + Math.floor(random() * 40), Math.floor(random() * 12),
                         1 + Math.floor(random() * 27)),
    });
  }
  const entries = [];
  for (const spec of specs) {
    entries.push(await zlib.ZipEntry.create(spec.name, spec.data, {
      method: spec.method, mode: spec.mode, modified: spec.modified,
    }));
  }
  const chunks = [];
  for await (const chunk of zlib.createZipArchive(entries)) chunks.push(chunk);
  return { archive: Buffer.concat(chunks), specs };
}

test('random archives round-trip through ZipEntry.read, ZipBuffer, and ZipFile', async () => {
  const random = mulberry32(0xC0FFEE);
  const rounds = 8;
  const dir = await fs.mkdtemp(path.join(tmpdir.path, 'zlib-zip-property-'));
  try {
    for (let round = 0; round < rounds; round++) {
      const { archive, specs } = await buildTree(random, 6);
      const byName = new Map(specs.map((spec) => [spec.name, spec]));

      // Path 1: ZipEntry.read
      for (const entry of zlib.ZipEntry.read(archive)) {
        const spec = byName.get(entry.name);
        assert.ok(spec, `unexpected entry ${entry.name}`);
        assert.deepStrictEqual(await entry.content(), spec.data);
        assert.strictEqual(entry.mode, spec.mode);
      }

      // Path 2: ZipBuffer
      using zipBuffer = new zlib.ZipBuffer(archive);
      assert.strictEqual(zipBuffer.size, specs.length);
      for (const [name, entry] of zipBuffer) {
        const spec = byName.get(name);
        assert.deepStrictEqual(await entry.content(), spec.data);
      }

      // Path 3: ZipFile (disk-backed), including one streamed read.
      const filePath = path.join(dir, `round-${round}.zip`);
      await fs.writeFile(filePath, archive);
      const zipFile = await zlib.ZipFile.open(filePath);
      try {
        assert.strictEqual(zipFile.size, specs.length);
        for (const spec of specs) {
          const entry = await zipFile.get(spec.name);
          assert.deepStrictEqual(await entry.content(), spec.data);
        }
        const firstName = specs[0].name;
        const streamed = [];
        for await (const chunk of await zipFile.stream(firstName)) streamed.push(chunk);
        assert.deepStrictEqual(Buffer.concat(streamed), specs[0].data);
      } finally {
        await zipFile.close();
      }
    }
  } finally {
    await fs.rm(dir, { recursive: true, force: true });
  }
}, { timeout: 60_000 });

test('a streamed write round-trips through a streamed read', async () => {
  const random = mulberry32(0xBADF00D);
  const data = randomBuffer(random, 256 * 1024);
  async function* source() {
    for (let i = 0; i < data.length; i += 4096) {
      yield data.subarray(i, Math.min(i + 4096, data.length));
    }
  }
  const entry = zlib.ZipEntry.createStream('streamed.bin', source());
  const chunks = [];
  for await (const chunk of zlib.createZipArchive([entry])) chunks.push(chunk);
  const archive = Buffer.concat(chunks);

  const [read] = zlib.ZipEntry.read(archive);
  assert.strictEqual(read.name, 'streamed.bin');
  assert.strictEqual(read.size, data.length);
  const streamedOut = [];
  for await (const chunk of read.contentIterator()) streamedOut.push(chunk);
  assert.deepStrictEqual(Buffer.concat(streamedOut), data);
});
