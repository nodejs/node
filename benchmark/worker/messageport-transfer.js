'use strict';

const common = require('../common.js');
const { MessageChannel } = require('worker_threads');
const bench = common.createBenchmark(main, {
  transferCount: [1, 4, 16, 64],
  n: [2e4],
});

function main(conf) {
  const n = conf.n;
  const transferCount = conf.transferCount;

  const { port1, port2 } = new MessageChannel();
  port2.onmessage = () => {};

  bench.start();
  for (let i = 0; i < n; i++) {
    const buffers = [];
    for (let j = 0; j < transferCount; j++) buffers.push(new ArrayBuffer(8));
    port1.postMessage(buffers, buffers);
  }
  bench.end(n);
  port1.close();
}
