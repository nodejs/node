'use strict';

const common = require('../common');

const {
  async,
  unlinkSync,
  openSync,
  closeSync,
  copyFile
} = require('fs');
const path = require('path');
const { promisify } = require('util');
const Countdown = require('../../test/common/countdown');
const src = path.resolve(__dirname, '.removeme-benchmark-garbage');
const dest = path.resolve(__dirname, '.removeme-benchmark-garbage2');

const bench = common.createBenchmark(main, {
  n: [20000],
  method: ['legacy', 'promisify', 'promise']
});

function useLegacy(n) {
  const countdown = new Countdown(n, () => {
    bench.end(n);
    unlinkSync(src);
    unlinkSync(dest);
  });
  bench.start();
  for (let i = 0; i < n; i++) {
    copyFile(src, dest, (err) => countdown.dec());
  }
}

function usePromisify(n) {
  const _copyFile = promisify(copyFile);
  const countdown = new Countdown(n, () => {
    bench.end(n);
    unlinkSync(src);
    unlinkSync(dest);
  });
  bench.start();
  for (let i = 0; i < n; i++) {
    _copyFile(src, dest)
      .then(() => countdown.dec());
  }
}

function usePromise(n) {
  const _copyFile = async.copyFile;
  const countdown = new Countdown(n, () => {
    bench.end(n);
    unlinkSync(src);
    unlinkSync(dest);
  });
  bench.start();
  for (let i = 0; i < n; i++) {
    _copyFile(src, dest)
      .then(() => countdown.dec());
  }
}

function main(conf) {
  const n = conf.n >>> 0;
  const method = conf.method;

  closeSync(openSync(src, 'w'));

  switch (method) {
    case 'legacy': return useLegacy(n);
    case 'promisify': return usePromisify(n);
    case 'promise': return usePromise(n);
    default:
      throw new Error('invalid method');
  }
}
