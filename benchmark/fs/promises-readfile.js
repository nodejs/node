'use strict';

const common = require('../common');
const { readFile, async } = require('fs');
const { promisify } = require('util');
const Countdown = require('../../test/common/countdown');

const bench = common.createBenchmark(main, {
  n: [2000],
  file: [__filename, 'benchmark/fixtures/alice.html'],
  method: ['legacy', 'promisify', 'promise']
});

function useLegacy(file, n) {
  const countdown = new Countdown(n, () => {
    bench.end(n);
  });
  bench.start();
  for (let i = 0; i < n; i++) {
    readFile(file, () => countdown.dec());
  }
}

function usePromisify(file, n) {
  const _readFile = promisify(readFile);
  const countdown = new Countdown(n, () => {
    bench.end(n);
  });
  bench.start();
  for (let i = 0; i < n; i++) {
    _readFile(file).then(() => countdown.dec());
  }
}

function usePromise(file, n) {
  const _readFile = async.readFile;
  const countdown = new Countdown(n, () => {
    bench.end(n);
  });
  bench.start();
  for (let i = 0; i < n; i++) {
    _readFile(file).then(() => countdown.dec());
  }
}

function main(conf) {
  const n = conf.n >>> 0;
  const file = conf.file;
  const method = conf.method;

  switch (method) {
    case 'legacy': return useLegacy(file, n);
    case 'promisify': return usePromisify(file, n);
    case 'promise': return usePromise(file, n);
    default:
      throw new Error('invalid method');
  }
}
