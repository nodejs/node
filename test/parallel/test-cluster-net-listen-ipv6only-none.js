'use strict';

const common = require('../common');
if (!common.hasIPv6)
  common.skip('no IPv6 support');

const assert = require('assert');
const cluster = require('cluster');
const net = require('net');
const Countdown = require('../common/countdown');

// This test ensures that the `ipv6Only` option in `net.Server.listen()`
// works as expected when we use cluster with `SCHED_NONE` schedulingPolicy.
cluster.schedulingPolicy = cluster.SCHED_NONE;
const host = '::';
const WORKER_ACCOUNT = 3;

if (cluster.isMaster) {
  const workers = new Map();
  let address;

  const countdown = new Countdown(WORKER_ACCOUNT, () => {
    // Make sure the `ipv6Only` option works.
    const server = net.createServer().listen({
      host: '0.0.0.0',
      port: address.port,
    }, common.mustCall(() => {
      // Exit.
      server.close();
      workers.forEach((worker) => {
        worker.disconnect();
      });
    }));
  });

  for (let i = 0; i < WORKER_ACCOUNT; i += 1) {
    const worker = cluster.fork().on('exit', common.mustCall((statusCode) => {
      assert.strictEqual(statusCode, 0);
    })).on('listening', common.mustCall((workerAddress) => {
      if (!address) {
        address = workerAddress;
      } else {
        assert.strictEqual(address.addressType, workerAddress.addressType);
        assert.strictEqual(address.host, workerAddress.host);
        assert.strictEqual(address.port, workerAddress.port);
      }
      countdown.dec();
    }));

    workers.set(i, worker);
  }
} else {
  net.createServer().listen({
    host,
    port: 0,
    ipv6Only: true,
  }, common.mustCall());
}
