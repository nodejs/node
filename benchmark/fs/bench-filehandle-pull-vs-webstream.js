// Compare FileHandle.createReadStream() vs readableWebStream() vs pull()
// reading a large file through two transforms: uppercase then gzip compress.
'use strict';

const common = require('../common.js');
const fs = require('fs');
const zlib = require('zlib');
const { Transform, Writable, pipeline } = require('stream');

const tmpdir = require('../../test/common/tmpdir');
tmpdir.refresh();
const filename = tmpdir.resolve(`.removeme-benchmark-garbage-${process.pid}`);

const bench = common.createBenchmark(main, {
  api: ['classic', 'webstream', 'pull'],
  filesize: [1024 * 1024, 16 * 1024 * 1024, 64 * 1024 * 1024],
  n: [5],
});

function main({ api, filesize, n }) {
  // Create the fixture file with repeating lowercase ASCII
  const chunk = Buffer.alloc(Math.min(filesize, 64 * 1024), 'abcdefghij');
  const fd = fs.openSync(filename, 'w');
  let remaining = filesize;
  while (remaining > 0) {
    const toWrite = Math.min(remaining, chunk.length);
    fs.writeSync(fd, chunk, 0, toWrite);
    remaining -= toWrite;
  }
  fs.closeSync(fd);

  if (api === 'classic') {
    benchClassic(n, filesize).then(() => cleanup());
  } else if (api === 'webstream') {
    benchWebStream(n, filesize).then(() => cleanup());
  } else {
    benchPull(n, filesize).then(() => cleanup());
  }
}

function cleanup() {
  try { fs.unlinkSync(filename); } catch { /* ignore */ }
}

// ---------------------------------------------------------------------------
// Classic streams path: createReadStream -> Transform (upper) -> createGzip
// ---------------------------------------------------------------------------
async function benchClassic(n, filesize) {
  // Warm up
  await runClassic();

  bench.start();
  let totalBytes = 0;
  for (let i = 0; i < n; i++) {
    totalBytes += await runClassic();
  }
  bench.end(totalBytes / (1024 * 1024));
}

function runClassic() {
  return new Promise((resolve, reject) => {
    const rs = fs.createReadStream(filename);

    // Transform 1: uppercase
    const upper = new Transform({
      transform(chunk, encoding, callback) {
        const buf = Buffer.allocUnsafe(chunk.length);
        for (let i = 0; i < chunk.length; i++) {
          const b = chunk[i];
          buf[i] = (b >= 0x61 && b <= 0x7a) ? b - 0x20 : b;
        }
        callback(null, buf);
      },
    });

    // Transform 2: gzip
    const gz = zlib.createGzip();

    // Sink: count compressed bytes
    let totalBytes = 0;
    const sink = new Writable({
      write(chunk, encoding, callback) {
        totalBytes += chunk.length;
        callback();
      },
    });

    pipeline(rs, upper, gz, sink, (err) => {
      if (err) reject(err);
      else resolve(totalBytes);
    });
  });
}

// ---------------------------------------------------------------------------
// WebStream path: readableWebStream -> TransformStream (upper) -> CompressionStream
// ---------------------------------------------------------------------------
async function benchWebStream(n, filesize) {
  // Warm up
  await runWebStream();

  bench.start();
  let totalBytes = 0;
  for (let i = 0; i < n; i++) {
    totalBytes += await runWebStream();
  }
  bench.end(totalBytes / (1024 * 1024));
}

async function runWebStream() {
  const fh = await fs.promises.open(filename, 'r');
  try {
    const rs = fh.readableWebStream();

    // Transform 1: uppercase
    const upper = new TransformStream({
      transform(chunk, controller) {
        const buf = new Uint8Array(chunk.length);
        for (let i = 0; i < chunk.length; i++) {
          const b = chunk[i];
          // a-z (0x61-0x7a) -> A-Z (0x41-0x5a)
          buf[i] = (b >= 0x61 && b <= 0x7a) ? b - 0x20 : b;
        }
        controller.enqueue(buf);
      },
    });

    // Transform 2: gzip via CompressionStream
    const compress = new CompressionStream('gzip');

    const output = rs.pipeThrough(upper).pipeThrough(compress);
    const reader = output.getReader();

    let totalBytes = 0;
    while (true) {
      const { done, value } = await reader.read();
      if (done) break;
      totalBytes += value.byteLength;
    }
    return totalBytes;
  } finally {
    await fh.close();
  }
}

// ---------------------------------------------------------------------------
// New streams path: pull() with uppercase transform + gzip transform
// ---------------------------------------------------------------------------
async function benchPull(n, filesize) {
  const { pull, compressGzip } = require('stream/new');

  // Warm up
  await runPull(pull, compressGzip);

  bench.start();
  let totalBytes = 0;
  for (let i = 0; i < n; i++) {
    totalBytes += await runPull(pull, compressGzip);
  }
  bench.end(totalBytes / (1024 * 1024));
}

async function runPull(pull, compressGzip) {
  const fh = await fs.promises.open(filename, 'r');
  try {
    // Stateless transform: uppercase each chunk in the batch
    const upper = (chunks) => {
      if (chunks === null) return null;
      const out = new Array(chunks.length);
      for (let j = 0; j < chunks.length; j++) {
        const src = chunks[j];
        const buf = new Uint8Array(src.length);
        for (let i = 0; i < src.length; i++) {
          const b = src[i];
          buf[i] = (b >= 0x61 && b <= 0x7a) ? b - 0x20 : b;
        }
        out[j] = buf;
      }
      return out;
    };

    const readable = fh.pull(upper, compressGzip());

    // Count bytes symmetrically with the classic path (no final
    // concatenation into a single buffer).
    let totalBytes = 0;
    for await (const chunks of readable) {
      for (let i = 0; i < chunks.length; i++) {
        totalBytes += chunks[i].byteLength;
      }
    }
    return totalBytes;
  } finally {
    await fh.close();
  }
}
