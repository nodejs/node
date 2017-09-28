'use strict';

const common = require('../common');
const {
  chmod,
  async,
  unlinkSync,
  openSync,
  closeSync
} = require('fs');
const path = require('path');
const { promisify } = require('util');
const Countdown = require('../../test/common/countdown');
const filename = path.resolve(__dirname, '.removeme-benchmark-garbage');
const mode = process.platform === 'win32' ? 0o600 : 0o644;

const bench = common.createBenchmark(main, {
  n: [20e4],
  method: ['legacy', 'promisify', 'promise']
});

function useLegacy(n) {
  const countdown = new Countdown(n, () => {
    bench.end(n);
    unlinkSync(filename);
  });
  bench.start();
  for (let i = 0; i < n; i++) {
    chmod(filename, mode, () => countdown.dec());
  }
}

function usePromisify(n) {
  const _chmod = promisify(chmod);
  const countdown = new Countdown(n, () => {
    bench.end(n);
    unlinkSync(filename);
  });
  bench.start();
  for (let i = 0; i < n; i++) {
    _chmod(filename, mode).then(() => countdown.dec());
  }
}

function usePromise(n) {
  const _chmod = async.chmod;
  const countdown = new Countdown(n, () => {
    bench.end(n);
    unlinkSync(filename);
  });
  bench.start();
  for (let i = 0; i < n; i++) {
    _chmod(filename, mode).then(() => countdown.dec());
  }
}

function main(conf) {
  const n = conf.n >>> 0;
  const method = conf.method;

  closeSync(openSync(filename, 'w'));

  switch (method) {
    case 'legacy': return useLegacy(n);
    case 'promisify': return usePromisify(n);
    case 'promise': return usePromise(n);
    default:
      throw new Error('invalid method');
  }
}
