'use strict';

const common = require('../common');

const {
  async,
  unlinkSync,
  open,
  close
} = require('fs');
const path = require('path');
const { promisify } = require('util');
const Countdown = require('../../test/common/countdown');
const filename = path.resolve(__dirname, '.removeme-benchmark-garbage');

const bench = common.createBenchmark(main, {
  n: [20000],
  method: ['legacy', 'promisify', 'promise']
});

function useLegacy(n) {
  const countdown = new Countdown(n, () => {
    bench.end(n);
    unlinkSync(filename);
  });
  bench.start();
  for (let i = 0; i < n; i++) {
    open(filename, 'w', (err, fd) => {
      close(fd, () => {
        countdown.dec();
      });
    });
  }
}

function usePromisify(n) {
  const _open = promisify(open);
  const _close = promisify(close);
  const countdown = new Countdown(n, () => {
    bench.end(n);
    unlinkSync(filename);
  });
  bench.start();
  for (let i = 0; i < n; i++) {
    _open(filename, 'w')
      .then(_close)
      .then(() => countdown.dec());
  }
}

function usePromise(n) {
  const _open = async.open;
  const _close = async.close;
  const countdown = new Countdown(n, () => {
    bench.end(n);
    unlinkSync(filename);
  });
  bench.start();
  for (let i = 0; i < n; i++) {
    _open(filename, 'w')
      .then(_close)
      .then(() => countdown.dec());
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
