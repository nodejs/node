'use strict';
const common = require('../common.js');
const {
  Readable,
  Writable,
} = require('node:stream');
const {
  ReadableStream,
  WritableStream,
} = require('node:stream/web');

const bench = common.createBenchmark(main, {
  n: [1e5],
  kind: [
    'readable-to-web',
    'readable-from-web',
    'writable-to-web',
    'writable-from-web',
  ],
});

async function readableToWeb(n) {
  const chunk = Buffer.alloc(1024);
  let i = 0;
  const streamReadable = new Readable({
    read() {
      if (i++ < n)
        this.push(chunk);
      else
        this.push(null);
    },
  });
  const reader = Readable.toWeb(streamReadable).getReader();
  bench.start();
  while (!(await reader.read()).done);
  bench.end(n);
}

function readableFromWeb(n) {
  const chunk = Buffer.alloc(1024);
  let i = 0;
  const readableStream = new ReadableStream({
    pull(controller) {
      if (i++ < n)
        controller.enqueue(chunk);
      else
        controller.close();
    },
  });
  const streamReadable = Readable.fromWeb(readableStream);
  bench.start();
  streamReadable.on('data', () => {});
  streamReadable.on('end', () => bench.end(n));
}

async function writableToWeb(n) {
  const chunk = Buffer.alloc(1024);
  const streamWritable = new Writable({
    write(chunk, encoding, callback) {
      callback();
    },
  });
  const writer = Writable.toWeb(streamWritable).getWriter();
  bench.start();
  for (let i = 0; i < n; i++)
    await writer.write(chunk);
  await writer.close();
  bench.end(n);
}

function writableFromWeb(n) {
  const chunk = Buffer.alloc(1024);
  const writableStream = new WritableStream({
    write() {},
  });
  const streamWritable = Writable.fromWeb(writableStream);
  bench.start();
  let i = 0;
  function writeLoop() {
    while (i++ < n) {
      if (!streamWritable.write(chunk)) {
        streamWritable.once('drain', writeLoop);
        return;
      }
    }
    streamWritable.end(() => bench.end(n));
  }
  writeLoop();
}

function main({ n, kind }) {
  switch (kind) {
    case 'readable-to-web':
      readableToWeb(n);
      break;
    case 'readable-from-web':
      readableFromWeb(n);
      break;
    case 'writable-to-web':
      writableToWeb(n);
      break;
    case 'writable-from-web':
      writableFromWeb(n);
      break;
  }
}
