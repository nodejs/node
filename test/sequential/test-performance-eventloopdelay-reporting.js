// Flags: --expose-internals
'use strict';

const common = require('../common');
const assert = require('assert');
const net = require('net');
const {
  monitorEventLoopDelay
} = require('perf_hooks');
const { sleep } = require('internal/util');

const ASYNC_SCHEDULERS = [
  (cb) => setTimeout(cb, common.platformTimeout(10)),
  (cb) => {
    const server = net.createServer((s) => {
      s.destroy();
      server.close();

      cb();
    });
    server.listen(0, () => {
      const { port, address: host } = server.address();
      net.connect(port, host, () => {
      });
    });
  },
];

async function testSingle(resolution, callback) {
  const histogram = monitorEventLoopDelay({ resolution });
  histogram.enable();

  for (const schedule of ASYNC_SCHEDULERS) {
    histogram.reset();

    const msSleep = common.platformTimeout(100);

    await new Promise((resolve, reject) => {
      schedule(() => {
        sleep(msSleep);
        resolve();
      });
    });

    // Let the new event loop tick start
    await new Promise((resolve, reject) => setTimeout(resolve, 10));

    // Convert sleep time to nanoseconds
    const nsSleep = msSleep * 1e6;

    // We are generous
    assert(histogram.max >= 0.75 * nsSleep,
      `${histogram.max} must be greater or equal to ${0.75 * nsSleep}`);
  }

  histogram.disable();
}

async function test() {
  await testSingle(0);
  await testSingle(1);
}

test();
