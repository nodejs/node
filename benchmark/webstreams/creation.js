'use strict';
const common = require('../common.js');
const {
  ReadableStream,
  TransformStream,
  WritableStream,
} = require('node:stream/web');
const assert = require('assert');

const bench = common.createBenchmark(main, {
  n: [50e3],
  kind: ['ReadableStream', 'TransformStream', 'WritableStream'],
});

let rs, ws, ts;

function main({ n, kind }) {
  switch (kind) {
    case 'ReadableStream':
      bench.start();
      for (let i = 0; i < n; ++i)
        rs = new ReadableStream();
      bench.end(n);

      // Avoid V8 deadcode (elimination)
      assert.ok(rs);
      break;
    case 'WritableStream':
      bench.start();
      for (let i = 0; i < n; ++i)
        ws = new WritableStream();
      bench.end(n);

      // Avoid V8 deadcode (elimination)
      assert.ok(ws);
      break;
    case 'TransformStream':
      bench.start();
      for (let i = 0; i < n; ++i)
        ts = new TransformStream();
      bench.end(n);

      // Avoid V8 deadcode (elimination)
      assert.ok(ts);
      break;
    default:
      throw new Error('Invalid kind');
  }
}
