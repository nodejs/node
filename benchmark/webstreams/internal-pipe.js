'use strict';
const common = require('../common.js');
const fsp = require('fs/promises');
const path = require('path');
const os = require('os');
const { pipeline } = require('stream/promises');
const {
  ReadableStream,
  WritableStream,
} = require('node:stream/web');

const bench = common.createBenchmark(main, {
  type: [
    'node-streams',
    'webstream-js',
    'webstream-file-read',
  ],
  size: [1024, 16384, 65536],
  n: [1e4, 1e5],
});

async function main({ type, size, n }) {
  const chunk = Buffer.alloc(size, 'x');
  const totalBytes = size * n;

  switch (type) {
    case 'node-streams': {
      // Baseline: Node.js streams
      let received = 0;
      const readable = new (require('stream').Readable)({
        read() {
          for (let i = 0; i < 100 && received < n; i++) {
            this.push(chunk);
            received++;
          }
          if (received >= n) this.push(null);
        },
      });

      const writable = new (require('stream').Writable)({
        write(data, enc, cb) { cb(); },
      });

      bench.start();
      await pipeline(readable, writable);
      bench.end(totalBytes);
      break;
    }

    case 'webstream-js': {
      // Web streams with pure JS source/sink
      let sent = 0;
      const rs = new ReadableStream({
        pull(controller) {
          if (sent++ < n) {
            controller.enqueue(chunk);
          } else {
            controller.close();
          }
        },
      });

      const ws = new WritableStream({
        write() {},
        close() { bench.end(totalBytes); },
      });

      bench.start();
      await rs.pipeTo(ws);
      break;
    }

    case 'webstream-file-read': {
      // Create a temporary file with test data
      const tmpDir = os.tmpdir();
      const tmpFile = path.join(tmpDir, `bench-webstream-${process.pid}.tmp`);

      // Write test data to file
      const fd = await fsp.open(tmpFile, 'w');
      for (let i = 0; i < n; i++) {
        await fd.write(chunk);
      }
      await fd.close();

      // Read using readableWebStream
      const readFd = await fsp.open(tmpFile, 'r');
      const rs = readFd.readableWebStream({ type: 'bytes' });

      const ws = new WritableStream({
        write() {},
        close() {
          bench.end(totalBytes);
          // Cleanup
          readFd.close().then(() => fsp.unlink(tmpFile));
        },
      });

      bench.start();
      await rs.pipeTo(ws);
      break;
    }

    default:
      throw new Error(`Unknown type: ${type}`);
  }
}
