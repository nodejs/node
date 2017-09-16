'use strict';

const common = require('../common');

if (process.platform === 'win32') {
  console.error('Benchmark unsupported on Windows');
  process.exit(1);
}

const {
  chown,
  async,
  unlinkSync,
  openSync,
  closeSync
} = require('fs');
const path = require('path');
const { promisify } = require('util');
const Countdown = require('../../test/common/countdown');
const filename = path.resolve(__dirname, '.removeme-benchmark-garbage');
const uid = process.getuid();
const gid = process.getgid();

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
    chown(filename, uid, gid, () => countdown.dec());
  }
}

function usePromisify(n) {
  const _chown = promisify(chown);
  const countdown = new Countdown(n, () => {
    bench.end(n);
    unlinkSync(filename);
  });
  bench.start();
  for (let i = 0; i < n; i++) {
    _chown(filename, uid, gid).then(() => countdown.dec());
  }
}

function usePromise(n) {
  const _chown = async.chown;
  const countdown = new Countdown(n, () => {
    bench.end(n);
    unlinkSync(filename);
  });
  bench.start();
  for (let i = 0; i < n; i++) {
    _chown(filename, uid, gid).then(() => countdown.dec());
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
