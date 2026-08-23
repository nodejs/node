'use strict';

// PGO Training Script: Compression (zlib/brotli)
//
// HTTP response compression is used on virtually every production Node.js server.
// This script exercises:
// - gzip compression/decompression (the most common HTTP content-encoding)
// - deflate compression/decompression
// - brotli compression/decompression (modern, better ratio)
// - Streaming compression (piped through HTTP responses)
// - One-shot compression (in-memory buffers)
// - Various compression levels and data types
// - CRC-32 computation
//
// This exercises: zlib C library, brotli C library, libuv thread pool
// (async compression), stream infrastructure, Buffer allocation.

const zlib = require('zlib');
const crypto = require('crypto');
const { pipeline } = require('stream/promises');
const { Readable, Writable } = require('stream');

const DURATION_MS = parseInt(process.env.PGO_TRAINING_DURATION, 10) || 15_000;

// Test data of different types and sizes (compression ratio varies)
const JSON_DATA = Buffer.from(
  JSON.stringify({
    users: Array.from({ length: 200 }, (_, i) => ({
      id: i,
      name: `User ${i}`,
      email: `user${i}@example.com`,
      role: ['admin', 'editor', 'viewer'][i % 3],
      active: i % 4 !== 0,
      profile: {
        bio: `This is the biography for user ${i}. It contains some repetitive text to demonstrate compression.`,
        location: ['New York', 'London', 'Tokyo', 'Berlin', 'Sydney'][i % 5],
        joinDate: `2024-${String((i % 12) + 1).padStart(2, '0')}-${String((i % 28) + 1).padStart(2, '0')}`,
      },
    })),
  }),
);

const HTML_DATA = Buffer.from(`<!DOCTYPE html>
<html lang="en">
<head><meta charset="UTF-8"><title>Test Page</title></head>
<body>
${Array.from(
  { length: 100 },
  (_, i) => `
<div class="card" id="card-${i}">
  <h2 class="card-title">Card Title ${i}</h2>
  <p class="card-body">This is the body text for card number ${i}. It contains enough text to be meaningful for compression benchmarks.</p>
  <div class="card-footer">
    <button class="btn btn-primary">Action ${i}</button>
    <span class="badge">${i % 5}</span>
  </div>
</div>`,
).join('\n')}
</body></html>`);

const CSS_DATA = Buffer.from(
  Array.from(
    { length: 200 },
    (_, i) => `
.component-${i} { display: flex; align-items: center; padding: ${i}px; margin: ${i % 20}px; }
.component-${i}:hover { background-color: #${String(i * 111)
      .padStart(6, '0')
      .slice(0, 6)}; transition: all 0.3s ease; }
.component-${i} .title { font-size: ${12 + (i % 8)}px; font-weight: ${i % 2 === 0 ? 'bold' : 'normal'}; }
.component-${i} .description { color: #666; line-height: 1.5; max-width: ${200 + i * 5}px; }
`,
  ).join('\n'),
);

const JS_DATA = Buffer.from(
  Array.from(
    { length: 100 },
    (_, i) => `
function handler${i}(req, res) {
  const data = req.body;
  if (!data || !data.id) {
    return res.status(400).json({ error: 'Missing id' });
  }
  const result = processData${i}(data);
  return res.json({ success: true, data: result, timestamp: Date.now() });
}
function processData${i}(data) {
  return { ...data, processed: true, handler: 'handler${i}' };
}
module.exports = { handler${i}, processData${i} };
`,
  ).join('\n'),
);

// Binary data (images, wasm — less compressible)
const BINARY_DATA = crypto.randomBytes(32768);

const TEST_DATA = [
  { name: 'JSON', data: JSON_DATA, weight: 5 },
  { name: 'HTML', data: HTML_DATA, weight: 3 },
  { name: 'CSS', data: CSS_DATA, weight: 2 },
  { name: 'JS', data: JS_DATA, weight: 2 },
  { name: 'Binary', data: BINARY_DATA, weight: 1 },
];

const weightedData = [];
for (const item of TEST_DATA) {
  for (let i = 0; i < item.weight; i++) {
    weightedData.push(item);
  }
}

// Workload 1: Gzip compress/decompress (one-shot, most common pattern)
async function workloadGzip(iterations) {
  let ops = 0;

  for (let i = 0; i < iterations; i++) {
    const item = weightedData[i % weightedData.length];

    // Compress (HTTP response compression)
    const compressed = await new Promise((resolve, reject) => {
      zlib.gzip(item.data, (err, result) => {
        if (err) reject(err);
        else resolve(result);
      });
    });
    ops++;

    // Decompress (HTTP response decompression on client)
    await new Promise((resolve, reject) => {
      zlib.gunzip(compressed, (err, result) => {
        if (err) reject(err);
        else resolve(result);
      });
    });
    ops++;

    // Sync variant (used in some build tools)
    if (i % 5 === 0) {
      const compSync = zlib.gzipSync(item.data);
      zlib.gunzipSync(compSync);
      ops += 2;
    }

    // Different compression levels
    if (i % 3 === 0) {
      await new Promise((resolve, reject) => {
        zlib.gzip(item.data, { level: 1 }, (err, result) => {
          // Fast
          if (err) reject(err);
          else resolve(result);
        });
      });
      ops++;
    }
    if (i % 7 === 0) {
      await new Promise((resolve, reject) => {
        zlib.gzip(item.data, { level: 9 }, (err, result) => {
          // Best
          if (err) reject(err);
          else resolve(result);
        });
      });
      ops++;
    }
  }
  return ops;
}

// Workload 2: Deflate compress/decompress
async function workloadDeflate(iterations) {
  let ops = 0;

  for (let i = 0; i < iterations; i++) {
    const item = weightedData[i % weightedData.length];

    // deflate (used in some HTTP implementations)
    const compressed = await new Promise((resolve, reject) => {
      zlib.deflate(item.data, (err, result) => {
        if (err) reject(err);
        else resolve(result);
      });
    });
    ops++;

    await new Promise((resolve, reject) => {
      zlib.inflate(compressed, (err, result) => {
        if (err) reject(err);
        else resolve(result);
      });
    });
    ops++;

    // deflateRaw (no header — used in some protocols)
    if (i % 3 === 0) {
      const raw = zlib.deflateRawSync(item.data);
      zlib.inflateRawSync(raw);
      ops += 2;
    }
  }
  return ops;
}

// Workload 3: Brotli compress/decompress (modern HTTP content-encoding)
async function workloadBrotli(iterations) {
  let ops = 0;

  for (let i = 0; i < iterations; i++) {
    const item = weightedData[i % weightedData.length];

    // Brotli compress (response compression for supported clients)
    const compressed = await new Promise((resolve, reject) => {
      zlib.brotliCompress(item.data, (err, result) => {
        if (err) reject(err);
        else resolve(result);
      });
    });
    ops++;

    // Brotli decompress
    await new Promise((resolve, reject) => {
      zlib.brotliDecompress(compressed, (err, result) => {
        if (err) reject(err);
        else resolve(result);
      });
    });
    ops++;

    // Different quality levels
    if (i % 3 === 0) {
      await new Promise((resolve, reject) => {
        zlib.brotliCompress(
          item.data,
          {
            params: { [zlib.constants.BROTLI_PARAM_QUALITY]: 1 }, // Fast
          },
          (err, result) => {
            if (err) reject(err);
            else resolve(result);
          },
        );
      });
      ops++;
    }
  }
  return ops;
}

// Workload 4: Streaming compression (piped HTTP responses)
async function workloadStreamCompress(iterations) {
  let ops = 0;

  for (let i = 0; i < iterations; i++) {
    const item = weightedData[i % weightedData.length];

    // Gzip stream (Express compression middleware pattern)
    await pipeline(
      Readable.from([item.data]),
      zlib.createGzip(),
      new Writable({
        write(chunk, encoding, callback) {
          callback();
        },
      }),
    );
    ops++;

    // Brotli stream
    if (i % 2 === 0) {
      await pipeline(
        Readable.from([item.data]),
        zlib.createBrotliCompress(),
        new Writable({
          write(chunk, encoding, callback) {
            callback();
          },
        }),
      );
      ops++;
    }

    // Chunked stream compression (large response simulation)
    if (i % 3 === 0) {
      const chunkSize = 4096;
      const chunks = [];
      for (let offset = 0; offset < item.data.length; offset += chunkSize) {
        chunks.push(
          item.data.subarray(
            offset,
            Math.min(offset + chunkSize, item.data.length),
          ),
        );
      }

      await pipeline(
        Readable.from(chunks),
        zlib.createGzip({ flush: zlib.constants.Z_SYNC_FLUSH }),
        new Writable({
          write(chunk, encoding, callback) {
            callback();
          },
        }),
      );
      ops++;
    }
  }
  return ops;
}

// Workload 5: CRC-32 (used in gzip, PNG, etc.)
function workloadCRC32(iterations) {
  let ops = 0;

  for (let i = 0; i < iterations; i++) {
    const item = weightedData[i % weightedData.length];
    zlib.crc32(item.data);
    ops++;

    // Incremental CRC (streaming pattern)
    if (i % 3 === 0) {
      const chunkSize = 1024;
      let crc = 0;
      for (let offset = 0; offset < item.data.length; offset += chunkSize) {
        const chunk = item.data.subarray(
          offset,
          Math.min(offset + chunkSize, item.data.length),
        );
        crc = zlib.crc32(chunk, crc);
      }
      ops++;
    }
  }
  return ops;
}

// Workload 6: Compression object creation (middleware initialization)
function workloadCompressionCreation(iterations) {
  let ops = 0;

  for (let i = 0; i < iterations; i++) {
    // Express compression middleware creates these per-request
    zlib.createGzip();
    zlib.createGunzip();
    zlib.createDeflate();
    zlib.createInflate();
    zlib.createBrotliCompress();
    zlib.createBrotliDecompress();
    ops += 6;

    // With options (common in production)
    zlib.createGzip({ level: 6, memLevel: 8, windowBits: 15 });
    zlib.createBrotliCompress({
      params: {
        [zlib.constants.BROTLI_PARAM_QUALITY]: 4,
        [zlib.constants.BROTLI_PARAM_MODE]: zlib.constants.BROTLI_MODE_TEXT,
      },
    });
    ops += 2;
  }
  return ops;
}

async function main() {
  console.log('[pgo-compression] Starting compression workload...');
  const startTime = Date.now();
  let totalOps = 0;
  let round = 0;

  const remaining = () => DURATION_MS - (Date.now() - startTime);

  while (remaining() > 0) {
    round++;
    const scale = Math.max(0.1, remaining() / DURATION_MS);
    const iterScale = (base) => Math.max(1, Math.floor(base * scale));

    // Gzip (most common — Accept-Encoding: gzip is universal)
    if (round === 1) console.log('[pgo-compression] Running gzip...');
    totalOps += await workloadGzip(iterScale(100));
    if (remaining() <= 0) break;

    // Deflate
    if (round === 1) console.log('[pgo-compression] Running deflate...');
    totalOps += await workloadDeflate(iterScale(50));
    if (remaining() <= 0) break;

    // Brotli (growing rapidly)
    if (round === 1) console.log('[pgo-compression] Running brotli...');
    totalOps += await workloadBrotli(iterScale(50));
    if (remaining() <= 0) break;

    // Streaming compression
    if (round === 1)
      console.log('[pgo-compression] Running stream compression...');
    totalOps += await workloadStreamCompress(iterScale(30));
    if (remaining() <= 0) break;

    // CRC-32
    if (round === 1) console.log('[pgo-compression] Running CRC-32...');
    totalOps += workloadCRC32(iterScale(1000));
    if (remaining() <= 0) break;

    // Object creation
    if (round === 1)
      console.log('[pgo-compression] Running compression object creation...');
    totalOps += workloadCompressionCreation(iterScale(500));
  }

  const elapsed = (Date.now() - startTime) / 1000;
  console.log(
    `[pgo-compression] Completed ${totalOps} ops in ${elapsed.toFixed(1)}s (${(totalOps / elapsed).toFixed(0)} ops/s) [${round} rounds]`,
  );
}

main().catch((err) => {
  console.error('[pgo-compression] Error:', err);
  process.exit(1);
});
