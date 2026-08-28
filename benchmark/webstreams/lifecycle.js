'use strict';
const common = require('../common.js');
const assert = require('assert');
const {
  ReadableStream,
  WritableStream,
  TransformStream,
} = require('node:stream/web');

const bench = common.createBenchmark(main, {
  n: [5e4],
  kind: ['readable', 'pipe-to', 'pipe-through'],
});

const chunk = Buffer.alloc(1024);

function makeSource() {
  let i = 0;
  return {
    pull(controller) {
      if (i++ < 4)
        controller.enqueue(chunk);
      else
        controller.close();
    },
  };
}

async function readable(n) {
  let chunks = 0;
  bench.start();
  for (let i = 0; i < n; i++) {
    const reader = new ReadableStream(makeSource()).getReader();
    while (!(await reader.read()).done) chunks++;
  }
  bench.end(n);
  assert.strictEqual(chunks, n * 4);
}

async function pipeTo(n) {
  let chunks = 0;
  bench.start();
  for (let i = 0; i < n; i++) {
    await new ReadableStream(makeSource())
      .pipeTo(new WritableStream({ write() { chunks++; } }));
  }
  bench.end(n);
  assert.strictEqual(chunks, n * 4);
}

async function pipeThrough(n) {
  let chunks = 0;
  bench.start();
  for (let i = 0; i < n; i++) {
    const reader = new ReadableStream(makeSource())
      .pipeThrough(new TransformStream())
      .getReader();
    while (!(await reader.read()).done) chunks++;
  }
  bench.end(n);
  assert.strictEqual(chunks, n * 4);
}

function main({ n, kind }) {
  switch (kind) {
    case 'readable':
      readable(n);
      break;
    case 'pipe-to':
      pipeTo(n);
      break;
    case 'pipe-through':
      pipeThrough(n);
      break;
  }
}
