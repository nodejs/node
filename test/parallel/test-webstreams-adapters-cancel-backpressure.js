'use strict';
const common = require('../common');
const { PassThrough, Readable, pipeline } = require('node:stream');
const { WritableStream } = require('node:stream/web');
const { setTimeout } = require('node:timers/promises');

// Regression test for https://github.com/nodejs/node/issues/64529
// Readable.toWeb() uncaughtException when the stream is canceled during backpressure resume

process.on('uncaughtException', (err) => {
  console.error('Uncaught exception!', err);
  process.exit(1);
});

async function run() {
  for (let i = 0; i < 50; i++) {
    const src = new Readable({
      read() {
        this.push(Buffer.alloc(16 * 1024, 1));
        if ((this.bytes = (this.bytes || 0) + 16384) > 512 * 1024) this.push(null);
      },
    });
    const pt = new PassThrough({ highWaterMark: 16384 });
    pipeline(src, pt, () => {});
    const web = Readable.toWeb(pt);

    const ac = new AbortController();
    const writer = new WritableStream(
      {
        async write() {
          await setTimeout(1);
        }
      },
      { highWaterMark: 1 },
    );

    // Disconnect randomly to catch the exact tick window
    setTimeout(i % 10).then(() => ac.abort()).then(common.mustCall());

    try {
      await web.pipeTo(writer, { signal: ac.signal });
    } catch {
      // Ignore abort errors
    }

    await new Promise((r) => setImmediate(r));
  }
}

run().then(common.mustCall(() => {
  process.exit(0);
}));
