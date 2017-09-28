'use strict';

const common = require('../common');
const { access, async } = require('fs');
const { promisify } = require('util');
const Countdown = require('../../test/common/countdown');

const bench = common.createBenchmark(main, {
  n: [20e4],
  method: ['legacy', 'promisify', 'promise']
});

function useLegacy(n) {
  const countdown = new Countdown(n, () => {
    bench.end(n);
  });
  bench.start();
  for (let i = 0; i < n; i++) {
    access(__filename, () => countdown.dec());
  }
}

function usePromisify(n) {
  const _access = promisify(access);
  const countdown = new Countdown(n, () => {
    bench.end(n);
  });
  bench.start();
  for (let i = 0; i < n; i++) {
    _access(__filename).then(() => countdown.dec());
  }
}

function usePromise(n) {
  const _access = async.access;
  const countdown = new Countdown(n, () => {
    bench.end(n);
  });
  bench.start();
  for (let i = 0; i < n; i++) {
    _access(__filename).then(() => countdown.dec());
  }
}

function main(conf) {
  const n = conf.n >>> 0;
  const method = conf.method;

  switch (method) {
    case 'legacy': return useLegacy(n);
    case 'promisify': return usePromisify(n);
    case 'promise': return usePromise(n);
    default:
      throw new Error('invalid method');
  }
}
