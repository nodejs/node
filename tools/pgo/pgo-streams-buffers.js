'use strict';

/* eslint-disable no-void */

// PGO Training Script: Streams and Buffers
//
// Buffers and Streams are the backbone of all I/O in Node.js.
// This script exercises:
// - Buffer creation (from, alloc, allocUnsafe — most frequent allocations)
// - Buffer encoding conversion (utf8, base64, hex, latin1 — every HTTP body)
// - Buffer copy, slice, concat, compare, indexOf (data processing)
// - Readable streams (HTTP request bodies, file reads)
// - Writable streams (HTTP responses, file writes)
// - Transform streams (compression, encryption, data pipelines)
// - Pipe chains (the core streaming pattern)
// - Object mode streams (ORMs, data processing libraries)
// - Async iteration over streams (modern pattern)
//
// This exercises: Buffer C++ implementation, stream state machine,
// back-pressure handling, highWaterMark management, GC pressure.

const { Readable, Writable, Transform, PassThrough } = require('stream');
const { pipeline: pipelinePromise } = require('stream/promises');
const { Buffer } = require('buffer');
const crypto = require('crypto');

const DURATION_MS = parseInt(process.env.PGO_TRAINING_DURATION, 10) || 15_000;

// ============== BUFFER WORKLOADS ==============

// Workload 1: Buffer creation (the most frequent allocation in Node.js)
function workloadBufferCreation(iterations) {
  let ops = 0;
  const str =
    'Hello, World! This is a test string for buffer creation benchmarks.';
  const arr = Array.from({ length: 64 }, (_, i) => i);
  const uint8 = new Uint8Array(64);
  const ab = new ArrayBuffer(64);

  for (let i = 0; i < iterations; i++) {
    // Buffer.from string (most common — every HTTP body, every JSON parse input)
    Buffer.from(str);
    Buffer.from(str, 'utf8');
    Buffer.from(str, 'ascii');
    Buffer.from(str, 'latin1');
    ops += 4;

    // Buffer.from with various source types
    Buffer.from(arr);
    Buffer.from(uint8);
    Buffer.from(ab);
    Buffer.from(Buffer.alloc(64));
    ops += 4;

    // Buffer.alloc (zeroed — safe allocation)
    Buffer.alloc(16);
    Buffer.alloc(256);
    Buffer.alloc(4096);
    Buffer.alloc(64, 0xff);
    ops += 4;

    // Buffer.allocUnsafe (fast allocation — used in hot paths)
    Buffer.allocUnsafe(16);
    Buffer.allocUnsafe(256);
    Buffer.allocUnsafe(4096);
    Buffer.allocUnsafe(65536);
    ops += 4;

    // Buffer.concat (building response bodies)
    const chunks = [
      Buffer.from('chunk1'),
      Buffer.from('chunk2'),
      Buffer.from('chunk3'),
    ];
    Buffer.concat(chunks);
    ops++;
  }
  return ops;
}

// Workload 2: Buffer encoding/decoding (every HTTP interaction)
function workloadBufferEncoding(iterations) {
  let ops = 0;
  const testData = crypto.randomBytes(1024);
  const testString =
    'The quick brown fox jumps over the lazy dog. 日本語テスト 🎉';
  const base64Data = testData.toString('base64');
  const hexData = testData.toString('hex');
  const base64urlData = testData.toString('base64url');

  for (let i = 0; i < iterations; i++) {
    // UTF-8 encode/decode (dominant encoding for web)
    const utf8Buf = Buffer.from(testString, 'utf8');
    utf8Buf.toString('utf8');
    ops += 2;

    // Base64 encode/decode (binary data in JSON, data URIs, email attachments)
    testData.toString('base64');
    Buffer.from(base64Data, 'base64');
    ops += 2;

    // Base64url (JWT tokens)
    testData.toString('base64url');
    Buffer.from(base64urlData, 'base64url');
    ops += 2;

    // Hex encode/decode (crypto hashes, color codes, debugging)
    testData.toString('hex');
    Buffer.from(hexData, 'hex');
    ops += 2;

    // ASCII/Latin1 (HTTP headers, protocol parsing)
    Buffer.from(
      'HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n\r\n',
      'ascii',
    );
    Buffer.from('Set-Cookie: session=abc123; Path=/; HttpOnly', 'latin1');
    ops += 2;

    // Buffer.byteLength (Content-Length header computation)
    Buffer.byteLength(testString, 'utf8');
    Buffer.byteLength(base64Data, 'base64');
    Buffer.byteLength('simple ascii text');
    ops += 3;
  }
  return ops;
}

// Workload 3: Buffer operations (data processing patterns)
function workloadBufferOps(iterations) {
  let ops = 0;
  const buf1 = crypto.randomBytes(1024);
  const buf2 = crypto.randomBytes(1024);
  const needle = Buffer.from('test');

  for (let i = 0; i < iterations; i++) {
    // Buffer.compare (sorting, binary search)
    Buffer.compare(buf1, buf2);
    buf1.compare(buf2, 0, 100, 0, 100);
    ops += 2;

    // Buffer.equals
    buf1.equals(buf2);
    buf1.equals(buf1);
    ops += 2;

    // slice/subarray (zero-copy views — extremely common)
    buf1.subarray(0, 256);
    buf1.subarray(256, 512);
    buf1.subarray(512);
    ops += 3;

    // copy (building protocol messages)
    const dest = Buffer.allocUnsafe(2048);
    buf1.copy(dest, 0);
    buf2.copy(dest, 1024);
    ops += 2;

    // indexOf / includes (searching in binary data, parsers)
    buf1.indexOf(needle);
    buf1.includes(65); // ASCII 'A'
    buf1.lastIndexOf(0);
    ops += 3;

    // fill (clearing sensitive data, initializing)
    const fillBuf = Buffer.allocUnsafe(256);
    fillBuf.fill(0);
    fillBuf.fill('abc');
    ops += 2;

    // Read/write numeric values (protocol parsing, binary formats)
    buf1.readUInt32BE(0);
    buf1.readUInt32LE(0);
    buf1.readInt16BE(4);
    buf1.readFloatLE(8);
    buf1.readDoubleBE(16);
    dest.writeUInt32BE(i, 0);
    dest.writeInt16LE(i % 32768, 4);
    dest.writeFloatLE(i * 1.5, 8);
    ops += 8;

    // swap (endianness conversion)
    const swapBuf = Buffer.from(buf1.subarray(0, 64));
    swapBuf.swap16();
    swapBuf.swap32();
    ops += 2;
  }
  return ops;
}

// ============== STREAM WORKLOADS ==============

// Workload 4: Readable → Writable pipe (canonical throughput pattern)
async function workloadPipeChain(iterations) {
  let ops = 0;

  for (let i = 0; i < iterations; i++) {
    const chunkCount = 1000;
    const chunkSize = 1024;

    await new Promise((resolve) => {
      let pushed = 0;
      const source = new Readable({
        read() {
          if (pushed >= chunkCount) {
            this.push(null);
            return;
          }
          this.push(crypto.randomBytes(chunkSize));
          pushed++;
        },
      });

      const sink = new Writable({
        write(chunk, encoding, callback) {
          callback();
        },
      });

      sink.on('finish', () => {
        ops += chunkCount;
        resolve();
      });
      source.pipe(sink);
    });
  }
  return ops;
}

// Workload 5: Transform streams (compression/encryption pattern)
async function workloadTransform(iterations) {
  let ops = 0;

  for (let i = 0; i < iterations; i++) {
    await new Promise((resolve) => {
      let pushed = 0;
      const chunkCount = 500;
      const source = new Readable({
        read() {
          if (pushed >= chunkCount) {
            this.push(null);
            return;
          }
          this.push(
            Buffer.from(
              `{"id":${pushed},"data":"${crypto.randomBytes(32).toString('hex')}"}\n`,
            ),
          );
          pushed++;
        },
      });

      // Transform: parse JSON line by line (NDJSON processing pattern)
      const transformer = new Transform({
        transform(chunk, encoding, callback) {
          const lines = chunk.toString().split('\n').filter(Boolean);
          for (const line of lines) {
            try {
              const obj = JSON.parse(line);
              obj.processed = true;
              obj.timestamp = Date.now();
              this.push(JSON.stringify(obj) + '\n');
            } catch {
              /* skip invalid */
            }
          }
          callback();
        },
      });

      const sink = new Writable({
        write(chunk, encoding, callback) {
          callback();
        },
      });

      sink.on('finish', () => {
        ops += chunkCount;
        resolve();
      });
      source.pipe(transformer).pipe(sink);
    });
  }
  return ops;
}

// Workload 6: Object mode streams (database query result processing)
async function workloadObjectMode(iterations) {
  let ops = 0;

  for (let i = 0; i < iterations; i++) {
    await new Promise((resolve) => {
      let pushed = 0;
      const objCount = 200;

      const source = new Readable({
        objectMode: true,
        read() {
          if (pushed >= objCount) {
            this.push(null);
            return;
          }
          this.push({
            id: pushed,
            name: `Record ${pushed}`,
            value: Math.random() * 1000,
            tags: ['tag1', 'tag2'],
            metadata: { created: Date.now(), source: 'db' },
          });
          pushed++;
        },
      });

      // Transform: filter + map (like a database cursor)
      const filter = new Transform({
        objectMode: true,
        transform(obj, encoding, callback) {
          if (obj.value > 100) {
            callback(null, {
              ...obj,
              value: Math.round(obj.value * 100) / 100,
              qualified: true,
            });
          } else {
            callback();
          }
        },
      });

      const results = [];
      const collect = new Writable({
        objectMode: true,
        write(obj, encoding, callback) {
          results.push(obj);
          callback();
        },
      });

      collect.on('finish', () => {
        ops += objCount;
        resolve();
      });
      source.pipe(filter).pipe(collect);
    });
  }
  return ops;
}

// Workload 7: Async iteration over streams (modern Node.js pattern)
async function workloadAsyncIteration(iterations) {
  let ops = 0;

  for (let i = 0; i < iterations; i++) {
    let pushed = 0;
    const chunkCount = 500;

    const source = new Readable({
      read() {
        if (pushed >= chunkCount) {
          this.push(null);
          return;
        }
        this.push(
          Buffer.from(
            `Line ${pushed}: ${crypto.randomBytes(20).toString('hex')}\n`,
          ),
        );
        pushed++;
      },
    });

    let lines = 0;
    for await (const chunk of source) {
      lines += chunk.toString().split('\n').filter(Boolean).length;
    }
    ops += lines;
  }
  return ops;
}

// Workload 8: pipeline() with error handling (production pattern)
async function workloadPipeline(iterations) {
  let ops = 0;

  for (let i = 0; i < iterations; i++) {
    let pushed = 0;
    const chunkCount = 300;

    const source = new Readable({
      read() {
        if (pushed >= chunkCount) {
          this.push(null);
          return;
        }
        this.push(crypto.randomBytes(512));
        pushed++;
      },
    });

    const uppercase = new Transform({
      transform(chunk, encoding, callback) {
        callback(null, chunk.toString('hex').toUpperCase());
      },
    });

    let bytes = 0;
    const sink = new Writable({
      write(chunk, encoding, callback) {
        bytes += chunk.length;
        callback();
      },
    });

    await pipelinePromise(source, uppercase, sink);
    ops += chunkCount;
    void bytes;
  }
  return ops;
}

// Workload 9: PassThrough / Duplex (proxy patterns, tee)
async function workloadPassThrough(iterations) {
  let ops = 0;

  for (let i = 0; i < iterations; i++) {
    let pushed = 0;
    const chunkCount = 200;

    const source = new Readable({
      read() {
        if (pushed >= chunkCount) {
          this.push(null);
          return;
        }
        this.push(crypto.randomBytes(256));
        pushed++;
      },
    });

    // Tee pattern: split stream to two destinations
    const pt1 = new PassThrough();
    const pt2 = new PassThrough();

    let bytes1 = 0;
    let bytes2 = 0;
    const sink1 = new Writable({
      write(chunk, encoding, callback) {
        bytes1 += chunk.length;
        callback();
      },
    });
    const sink2 = new Writable({
      write(chunk, encoding, callback) {
        bytes2 += chunk.length;
        callback();
      },
    });

    source.pipe(pt1).pipe(sink1);
    source.pipe(pt2).pipe(sink2);

    await Promise.all([
      new Promise((r) => sink1.on('finish', r)),
      new Promise((r) => sink2.on('finish', r)),
    ]);
    ops += chunkCount * 2;
    void bytes1;
    void bytes2;
  }
  return ops;
}

// Workload 10: Readable.from() with generators (modern data source pattern)
async function workloadReadableFrom(iterations) {
  let ops = 0;

  for (let i = 0; i < iterations; i++) {
    // From array
    const arr = Array.from({ length: 100 }, (_, j) => `item-${j}\n`);
    const stream1 = Readable.from(arr);
    let data1 = '';
    for await (const chunk of stream1) {
      data1 += chunk;
    }
    ops += 100;
    void data1;

    // From async generator (database cursor simulation)
    async function* generateRows() {
      for (let j = 0; j < 100; j++) {
        yield JSON.stringify({ row: j, value: Math.random() }) + '\n';
      }
    }

    const stream2 = Readable.from(generateRows());
    let data2 = '';
    for await (const chunk of stream2) {
      data2 += chunk;
    }
    ops += 100;
    void data2;
  }
  return ops;
}

async function main() {
  console.log('[pgo-streams-buffers] Starting streams & buffers workload...');
  const startTime = Date.now();
  let totalOps = 0;
  let round = 0;

  const remaining = () => DURATION_MS - (Date.now() - startTime);

  while (remaining() > 0) {
    round++;
    const scale = Math.max(0.1, remaining() / DURATION_MS);
    const iterScale = (base) => Math.max(1, Math.floor(base * scale));

    // Buffer workloads (sync, fast — exercise C++ bindings extensively)
    if (round === 1)
      console.log('[pgo-streams-buffers] Running buffer creation...');
    totalOps += workloadBufferCreation(iterScale(2000));
    if (remaining() <= 0) break;

    if (round === 1)
      console.log('[pgo-streams-buffers] Running buffer encoding...');
    totalOps += workloadBufferEncoding(iterScale(1000));
    if (remaining() <= 0) break;

    if (round === 1)
      console.log('[pgo-streams-buffers] Running buffer operations...');
    totalOps += workloadBufferOps(iterScale(1000));
    if (remaining() <= 0) break;

    // Stream workloads (async, exercise event loop + back-pressure)
    if (round === 1)
      console.log('[pgo-streams-buffers] Running pipe chains...');
    totalOps += await workloadPipeChain(iterScale(5));
    if (remaining() <= 0) break;

    if (round === 1)
      console.log('[pgo-streams-buffers] Running transform streams...');
    totalOps += await workloadTransform(iterScale(5));
    if (remaining() <= 0) break;

    if (round === 1)
      console.log('[pgo-streams-buffers] Running object mode streams...');
    totalOps += await workloadObjectMode(iterScale(10));
    if (remaining() <= 0) break;

    if (round === 1)
      console.log('[pgo-streams-buffers] Running async iteration...');
    totalOps += await workloadAsyncIteration(iterScale(5));
    if (remaining() <= 0) break;

    if (round === 1) console.log('[pgo-streams-buffers] Running pipeline()...');
    totalOps += await workloadPipeline(iterScale(5));
    if (remaining() <= 0) break;

    if (round === 1)
      console.log('[pgo-streams-buffers] Running PassThrough...');
    totalOps += await workloadPassThrough(iterScale(5));
    if (remaining() <= 0) break;

    if (round === 1)
      console.log('[pgo-streams-buffers] Running Readable.from()...');
    totalOps += await workloadReadableFrom(iterScale(10));
  }

  const elapsed = (Date.now() - startTime) / 1000;
  console.log(
    `[pgo-streams-buffers] Completed ${totalOps} ops in ${elapsed.toFixed(1)}s (${(totalOps / elapsed).toFixed(0)} ops/s) [${round} rounds]`,
  );
}

main().catch((err) => {
  console.error('[pgo-streams-buffers] Error:', err);
  process.exit(1);
});
