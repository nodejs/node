'use strict';
const common = require('../common.js');
const {
  ReadableStream,
  ReadableStreamDefaultReader,
  ReadableStreamBYOBReader,
  TransformStream,
  WritableStream,
} = require('node:stream/web');
const assert = require('assert');

const bench = common.createBenchmark(main, {
  n: [50e3],
  kind: [
    'ReadableStream',
    'TransformStream',
    'WritableStream',

    'ReadableStreamDefaultReader',
    'ReadableStreamBYOBReader',

    'ReadableStream.tee',
  ],
});

let readableStream;
let transformStream;
let writableStream;
let readableStreamDefaultReader;
let readableStreamBYOBReader;
let teeResult;

function main({ n, kind }) {
  switch (kind) {
    case 'ReadableStream':
      bench.start();
      for (let i = 0; i < n; ++i)
        readableStream = new ReadableStream();
      bench.end(n);

      // Avoid V8 deadcode (elimination)
      assert.ok(readableStream);
      break;
    case 'WritableStream':
      bench.start();
      for (let i = 0; i < n; ++i)
        writableStream = new WritableStream();
      bench.end(n);

      // Avoid V8 deadcode (elimination)
      assert.ok(writableStream);
      break;
    case 'TransformStream':
      bench.start();
      for (let i = 0; i < n; ++i)
        transformStream = new TransformStream();
      bench.end(n);

      // Avoid V8 deadcode (elimination)
      assert.ok(transformStream);
      break;
    case 'ReadableStreamDefaultReader': {
      const readers = Array.from({ length: n }, () => new ReadableStream());

      bench.start();
      for (let i = 0; i < n; ++i)
        readableStreamDefaultReader = new ReadableStreamDefaultReader(readers[i]);
      bench.end(n);

      // Avoid V8 deadcode (elimination)
      assert.ok(readableStreamDefaultReader);
      break;
    }
    case 'ReadableStreamBYOBReader': {
      const readers = Array.from({ length: n }, () => new ReadableStream({ type: 'bytes' }));

      bench.start();
      for (let i = 0; i < n; ++i)
        readableStreamBYOBReader = new ReadableStreamBYOBReader(readers[i]);
      bench.end(n);

      // Avoid V8 deadcode (elimination)
      assert.ok(readableStreamBYOBReader);
      break;
    }
    case 'ReadableStream.tee': {
      const streams = Array.from({ length: n }, () => new ReadableStream());

      bench.start();
      for (let i = 0; i < n; ++i)
        teeResult = streams[i].tee();
      bench.end(n);

      // Avoid V8 deadcode (elimination)
      assert.ok(teeResult);
      break;
    }
    default:
      throw new Error('Invalid kind');
  }
}
