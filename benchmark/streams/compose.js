'use strict';
const common = require('../common.js');

const {
  PassThrough,
  Readable,
  Writable,
  compose,
} = require('node:stream');

const bench = common.createBenchmark(main, {
  type: ['creation', 'throughput'],
  n: [1, 1e3],
  streams: [100],
  chunks: [1e4],
}, {
  combinationFilter({ type, n }) {
    return type === 'creation' ? n === 1e3 : n === 1;
  },
  test: {
    n: [1, 1e3],
    type: ['creation', 'throughput'],
  },
});

function main({ type, n, streams, chunks }) {
  switch (type) {
    case 'creation':
      return benchCreation(n, streams);
    case 'throughput':
      return benchThroughput(n, streams, chunks);
  }
}

function benchCreation(n, numberOfPassThroughs) {
  const cachedPassThroughs = [];
  const cachedReadables = [];
  const cachedWritables = [];

  for (let i = 0; i < n; i++) {
    const passThroughs = [];

    for (let i = 0; i < numberOfPassThroughs; i++) {
      passThroughs.push(new PassThrough());
    }

    const readable = Readable.from(['hello', 'world']);
    const writable = new Writable({ objectMode: true, write: (chunk, encoding, cb) => cb() });

    cachedPassThroughs.push(passThroughs);
    cachedReadables.push(readable);
    cachedWritables.push(writable);
  }

  bench.start();
  for (let i = 0; i < n; i++) {
    const composed = compose(cachedReadables[i], ...cachedPassThroughs[i], cachedWritables[i]);
    composed.end();
  }
  bench.end(n);
}

function benchThroughput(n, numberOfPassThroughs, chunks) {
  const chunk = Buffer.alloc(1024);

  let i = 0;
  bench.start();

  function run() {
    if (i++ === n) {
      bench.end(n * chunks);
      return;
    }

    const passThroughs = [];
    for (let i = 0; i < numberOfPassThroughs; i++) {
      passThroughs.push(new PassThrough());
    }

    let remaining = chunks;
    const composed = compose(...passThroughs);
    composed.on('data', () => {});
    composed.on('end', run);

    write();

    function write() {
      while (remaining-- > 0) {
        if (!composed.write(chunk)) {
          composed.once('drain', write);
          return;
        }
      }
      composed.end();
    }
  }

  run();
}
