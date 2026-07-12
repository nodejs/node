'use strict';

require('../common');

const assert = require('node:assert');
const zlib = require('node:zlib');
const { test } = require('node:test');

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

async function buildSeedArchive() {
  const entries = [
    await zlib.ZipEntry.create('hello.txt', Buffer.from('Hello, world!'.repeat(30))),
    await zlib.ZipEntry.create('raw.bin', Buffer.from([1, 2, 3, 4, 5]), { method: 'store' }),
    await zlib.ZipEntry.create('empty/', Buffer.alloc(0)),
  ];
  const chunks = [];
  for await (const chunk of zlib.createZipArchive(entries, 'seed archive')) chunks.push(chunk);
  return Buffer.concat(chunks);
}

function mutate(random, seed) {
  const buf = Buffer.from(seed);
  const kind = Math.floor(random() * 4);
  if (kind === 0) {
    // Flip a handful of random bits.
    const flips = 1 + Math.floor(random() * 8);
    for (let i = 0; i < flips; i++) {
      buf[Math.floor(random() * buf.length)] ^= 1 << Math.floor(random() * 8);
    }
  } else if (kind === 1) {
    // Overwrite a random window with boundary-ish values.
    const boundary = [0x00, 0xff, 0x50, 0x4b][Math.floor(random() * 4)];
    const start = Math.floor(random() * buf.length);
    const len = Math.min(buf.length - start, 1 + Math.floor(random() * 8));
    buf.fill(boundary, start, start + len);
  } else if (kind === 2) {
    // Truncate.
    return buf.subarray(0, Math.floor(random() * buf.length));
  } else {
    // Extend with random padding.
    const pad = Buffer.allocUnsafe(1 + Math.floor(random() * 16));
    for (let i = 0; i < pad.length; i++) pad[i] = Math.floor(random() * 256);
    return Buffer.concat([buf, pad]);
  }
  return buf;
}

test('the parser only ever throws Error on mutated archives, never crashes or hangs', async () => {
  const seed = await buildSeedArchive();
  const random = mulberry32(0x5EED1234);
  const iterations = 3000;

  for (let i = 0; i < iterations; i++) {
    const candidate = mutate(random, seed);
    try {
      const entries = [...zlib.ZipEntry.read(candidate)];
      for (const entry of entries) {
        try {
          await entry.content();
        } catch (err) {
          assert.ok(err instanceof Error, `content() threw non-Error: ${err}`);
        }
      }
    } catch (err) {
      assert.ok(err instanceof Error, `read() threw non-Error: ${err}`);
    }

    try {
      using zipBuffer = new zlib.ZipBuffer(candidate);
      assert.ok(zipBuffer.size >= 0);
    } catch (err) {
      assert.ok(err instanceof Error, `ZipBuffer threw non-Error: ${err}`);
    }
  }
}, { timeout: 60_000 });
